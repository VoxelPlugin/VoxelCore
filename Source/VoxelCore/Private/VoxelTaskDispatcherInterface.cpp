// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskDispatcherInterface.h"

IVoxelTaskDispatcher* GVoxelGlobalTaskDispatcher = nullptr;

DEFINE_VOXEL_INSTANCE_COUNTER(IVoxelTaskDispatcher);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelTaskDispatcherManager : public FVoxelSingleton
{
public:
	FVoxelSharedCriticalSection CriticalSection;
	TVoxelSparseArray<IVoxelTaskDispatcher*> TaskDispatchers_RequiresLock;
	FVoxelCounter32 SerialCounter;

	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		GVoxelGlobalTaskDispatcher = new IVoxelTaskDispatcher(false);

		Voxel::OnFlushGameTasks.AddLambda([this](bool& bAnyTaskProcessed)
		{
			ProcessGameTasks(bAnyTaskProcessed);
		});

		FCoreDelegates::OnEnginePreExit.AddLambda([this]
		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			for (IVoxelTaskDispatcher* TaskDispatcher : TaskDispatchers_RequiresLock)
			{
				TaskDispatcher->bIsExiting.Set(true);
			}
		});
	}
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		bool bAnyTaskProcessed;
		ProcessGameTasks(bAnyTaskProcessed);
	}
	//~ End FVoxelSingleton Interface

	void ProcessGameTasks(bool& bAnyTaskProcessed)
	{
		VOXEL_FUNCTION_COUNTER();

		TVoxelArray<FVoxelTaskDispatcherStrongRef> StrongRefs;
		StrongRefs.Reserve(TaskDispatchers_RequiresLock.Num());
		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			for (IVoxelTaskDispatcher* TaskDispatcher : TaskDispatchers_RequiresLock)
			{
				StrongRefs.Emplace(*TaskDispatcher);
			}
		}

		for (const FVoxelTaskDispatcherStrongRef& StrongRef : StrongRefs)
		{
			StrongRef.Dispatcher.ProcessGameTasks(bAnyTaskProcessed);
		}
	}
};
FVoxelTaskDispatcherManager* GVoxelTaskDispatcherManager = new FVoxelTaskDispatcherManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TUniquePtr<FVoxelTaskDispatcherStrongRef> FVoxelTaskDispatcherWeakRef::Pin() const
{
	if (Index == -1)
	{
		checkVoxelSlow(Serial == -1)
		return {};
	}

	VOXEL_SCOPE_READ_LOCK(GVoxelTaskDispatcherManager->CriticalSection);

	if (!GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.IsAllocated(Index))
	{
		return {};
	}

	IVoxelTaskDispatcher* Dispatcher = GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock[Index];
	checkVoxelSlow(Dispatcher);
	checkVoxelSlow(Dispatcher->SelfWeakRef.Index == Index);

	if (Dispatcher->bIsExiting.Get() ||
		Dispatcher->SelfWeakRef.Serial != Serial)
	{
		return nullptr;
	}

	return MakeUnique<FVoxelTaskDispatcherStrongRef>(*Dispatcher);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTaskDispatcherStrongRef::FVoxelTaskDispatcherStrongRef(IVoxelTaskDispatcher& Dispatcher)
	: Dispatcher(Dispatcher)
{
	Dispatcher.NumStrongRefs.Increment();
}

FVoxelTaskDispatcherStrongRef::~FVoxelTaskDispatcherStrongRef()
{
	Dispatcher.NumStrongRefs.Decrement();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IVoxelTaskDispatcher::IVoxelTaskDispatcher(const bool bTrackPromisesCallstacks)
	: bTrackPromisesCallstacks(bTrackPromisesCallstacks)
{
	VOXEL_SCOPE_WRITE_LOCK(GVoxelTaskDispatcherManager->CriticalSection);

	SelfWeakRef.Index = GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.Add(this);
	SelfWeakRef.Serial = GVoxelTaskDispatcherManager->SerialCounter.Increment_ReturnNew();
}

IVoxelTaskDispatcher::~IVoxelTaskDispatcher()
{
	VOXEL_FUNCTION_COUNTER();

	bIsExiting.Set(true);

	while (true)
	{
		{
			VOXEL_SCOPE_LOCK(GameTasksCriticalSection);

			NumPendingTasks.Subtract(GameTasks_RequiresLock.Num());
			GameTasks_RequiresLock.Empty();
		}

		{
			VOXEL_SCOPE_LOCK(AsyncTasksCriticalSection);

			NumPendingTasks.Subtract(AsyncTasks_RequiresLock.Num());
			AsyncTasks_RequiresLock.Empty();
		}

		if (NumStrongRefs.Get() == 0 &&
			NumPendingTasks.Get() == 0)
		{
			if (GVoxelTaskDispatcherManager->CriticalSection.TryWriteLock())
			{
				if (NumStrongRefs.Get() == 0 &&
					NumPendingTasks.Get() == 0)
				{
					break;
				}
			}
		}

		FPlatformProcess::Yield();
	}
	check(NumStrongRefs.Get() == 0);
	check(NumPendingTasks.Get() == 0);
	check(NumLaunchedTasks.Get() == 0);

	check(GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock[SelfWeakRef.Index] == this);
	GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.RemoveAt(SelfWeakRef.Index);

	GVoxelTaskDispatcherManager->CriticalSection.WriteUnlock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelTaskDispatcher::Dispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
#if VOXEL_DEBUG
	Lambda = [this, Lambda = MoveTemp(Lambda)]
	{
		check(&FVoxelTaskDispatcherScope::Get() == this);
		Lambda();
	};
#endif

	if (bIsExiting.Get())
	{
		return;
	}

	switch (Thread)
	{
	default: VOXEL_ASSUME(false);
	case EVoxelFutureThread::AnyThread:
	{
		FVoxelTaskDispatcherScope Scope(*this);
		Lambda();
	}
	break;
	case EVoxelFutureThread::GameThread:
	{
		NumPendingTasks.Increment();

		VOXEL_SCOPE_LOCK(GameTasksCriticalSection);
		GameTasks_RequiresLock.Add(MoveTemp(Lambda));
	}
	break;
	case EVoxelFutureThread::RenderThread:
	{
		NumPendingTasks.Increment();

		// One Voxel::RenderTask per call, otherwise the command ordering can be incorrect
		Voxel::RenderTask_SkipDispatcher([this, Lambda = MoveTemp(Lambda)]
		{
			if (!bIsExiting.Get())
			{
				FVoxelTaskDispatcherScope Scope(*this);
				Lambda();
			}

			NumPendingTasks.Decrement();
		});
	}
	break;
	case EVoxelFutureThread::AsyncThread:
	{
		NumPendingTasks.Increment();

		if (NumLaunchedTasks.Get() < MaxLaunchedTasks)
		{
			LaunchTask(MoveTemp(Lambda));
			return;
		}

		{
			VOXEL_SCOPE_LOCK(AsyncTasksCriticalSection);
			AsyncTasks_RequiresLock.Add(MoveTemp(Lambda));
		}

		if (NumLaunchedTasks.Get() < MaxLaunchedTasks)
		{
			LaunchTasks();
		}
	}
	break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelTaskDispatcher::DumpToLog()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	LOG_VOXEL(Log, "Queued game tasks: %d", GameTasks_RequiresLock.Num());
	LOG_VOXEL(Log, "Queued async tasks: %d", AsyncTasks_RequiresLock.Num());
	LOG_VOXEL(Log, "Launched async tasks: %d", NumLaunchedTasks.Get());

	LOG_VOXEL(Log, "Num promises: %d", GetNumPromises());
	LOG_VOXEL(Log, "Num pending tasks: %d", NumPendingTasks.Get());

	TVoxelMap<FVoxelStackFrames, int32> StackFramesToCount;
	StackFramesToCount.Reserve(StackFrames_RequiresLock.Num());

	for (const FVoxelStackFrames& StackFrames : StackFrames_RequiresLock)
	{
		StackFramesToCount.FindOrAdd(StackFrames)++;
	}

	StackFramesToCount.ValueSort([](const int32 A, const int32 B)
	{
		return A > B;
	});

	for (const auto& It : StackFramesToCount)
	{
		LOG_VOXEL(Log, "x%d:", It.Value);

		for (const FString& Line : FVoxelUtilities::StackFramesToString(It.Key))
		{
			LOG_VOXEL(Log, "\t%s", *Line);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelTaskDispatcher::LaunchTasks()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(AsyncTasksCriticalSection);
	checkStatic(2 * AsyncTasks_RequiresLock.NumPerChunk == MaxLaunchedTasks);

	while (
		AsyncTasks_RequiresLock.Num() > 0 &&
		NumLaunchedTasks.Get() < MaxLaunchedTasks)
	{
		for (TVoxelUniqueFunction<void()>& Task : AsyncTasks_RequiresLock.PopFirstChunk())
		{
			LaunchTask(MoveTemp(Task));
		}
	}
}

void IVoxelTaskDispatcher::LaunchTask(TVoxelUniqueFunction<void()> Task)
{
	NumLaunchedTasks.Increment();

	UE::Tasks::Launch(
		TEXT("Voxel Task"),
		[this, Task = MoveTemp(Task)]
		{
			if (!bIsExiting.Get())
			{
				FVoxelTaskDispatcherScope Scope(*this);
				Task();
			}

			if (NumLaunchedTasks.Decrement_ReturnNew() < MaxLaunchedTasks)
			{
				LaunchTasks();
			}

			// Decrement allows us to be deleted, make sure to do it last
			NumPendingTasks.Decrement();
		},
		LowLevelTasks::ETaskPriority::BackgroundLow);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelTaskDispatcher::ProcessGameTasks(bool& bAnyTaskProcessed)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GameTasks;
	{
		VOXEL_SCOPE_LOCK(GameTasksCriticalSection);
		GameTasks = MoveTemp(GameTasks_RequiresLock);
	}

	if (GameTasks.Num() == 0)
	{
		return;
	}
	bAnyTaskProcessed = true;

	FVoxelTaskDispatcherScope Scope(*this);

	for (const TVoxelUniqueFunction<void()>& Task : GameTasks)
	{
		if (!bIsExiting.Get())
		{
			Task();
		}
		NumPendingTasks.Decrement();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const uint32 GVoxelTaskDispatcherScopeTLS = FPlatformTLS::AllocTlsSlot();