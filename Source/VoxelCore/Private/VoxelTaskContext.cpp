// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskContext.h"

FVoxelTaskContext* GVoxelGlobalTaskContext = nullptr;

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelTaskContext);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelTaskContextManager : public FVoxelSingleton
{
public:
	FVoxelSharedCriticalSection CriticalSection;
	TVoxelSparseArray<FVoxelTaskContext*> Contexts_RequiresLock;
	FVoxelCounter32 SerialCounter;

	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		GVoxelGlobalTaskContext = new FVoxelTaskContext(false);

		GOnVoxelModuleUnloaded_DoCleanup.AddLambda([]
		{
			delete GVoxelGlobalTaskContext;
			GVoxelGlobalTaskContext = nullptr;
		});

		Voxel::OnFlushGameTasks.AddLambda([this](bool& bAnyTaskProcessed)
		{
			ProcessGameTasks(bAnyTaskProcessed);
		});

		FCoreDelegates::OnEnginePreExit.AddLambda([this]
		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			for (FVoxelTaskContext* Context : Contexts_RequiresLock)
			{
				Context->bIsExiting.Set(true);
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

		TVoxelArray<FVoxelTaskContextStrongRef> StrongRefs;
		StrongRefs.Reserve(Contexts_RequiresLock.Num());
		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			for (FVoxelTaskContext* Context : Contexts_RequiresLock)
			{
				StrongRefs.Emplace(*Context);
			}
		}

		for (const FVoxelTaskContextStrongRef& StrongRef : StrongRefs)
		{
			StrongRef.Context.ProcessGameTasks(bAnyTaskProcessed);
		}
	}
};
FVoxelTaskContextManager* GVoxelTaskContextManager = new FVoxelTaskContextManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TUniquePtr<FVoxelTaskContextStrongRef> FVoxelTaskContextWeakRef::Pin() const
{
	if (Index == -1)
	{
		checkVoxelSlow(Serial == -1)
		return {};
	}

	VOXEL_SCOPE_READ_LOCK(GVoxelTaskContextManager->CriticalSection);

	if (!GVoxelTaskContextManager->Contexts_RequiresLock.IsAllocated(Index))
	{
		return {};
	}

	FVoxelTaskContext* Context = GVoxelTaskContextManager->Contexts_RequiresLock[Index];
	checkVoxelSlow(Context);
	checkVoxelSlow(Context->SelfWeakRef.Index == Index);

	if (Context->bIsExiting.Get() ||
		Context->SelfWeakRef.Serial != Serial)
	{
		return nullptr;
	}

	return MakeUnique<FVoxelTaskContextStrongRef>(*Context);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTaskContextStrongRef::FVoxelTaskContextStrongRef(FVoxelTaskContext& Context)
	: Context(Context)
{
	Context.NumStrongRefs.Increment();
}

FVoxelTaskContextStrongRef::~FVoxelTaskContextStrongRef()
{
	Context.NumStrongRefs.Decrement();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTaskContext::FVoxelTaskContext(const bool bTrackPromisesCallstacks)
	: bTrackPromisesCallstacks(bTrackPromisesCallstacks)
{
	VOXEL_SCOPE_WRITE_LOCK(GVoxelTaskContextManager->CriticalSection);

	SelfWeakRef.Index = GVoxelTaskContextManager->Contexts_RequiresLock.Add(this);
	SelfWeakRef.Serial = GVoxelTaskContextManager->SerialCounter.Increment_ReturnNew();
}

FVoxelTaskContext::~FVoxelTaskContext()
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
			if (GVoxelTaskContextManager->CriticalSection.TryWriteLock())
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

	check(GVoxelTaskContextManager->Contexts_RequiresLock[SelfWeakRef.Index] == this);
	GVoxelTaskContextManager->Contexts_RequiresLock.RemoveAt(SelfWeakRef.Index);

	GVoxelTaskContextManager->CriticalSection.WriteUnlock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTaskContext::Dispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
#if VOXEL_DEBUG
	Lambda = [this, Lambda = MoveTemp(Lambda)]
	{
		check(&FVoxelTaskScope::GetContext() == this);
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
		FVoxelTaskScope Scope(*this);
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

		// One ENQUEUE_RENDER_COMMAND per call, otherwise the command ordering can be incorrect
		ENQUEUE_RENDER_COMMAND(FVoxelTaskContext)([this, Lambda = MoveTemp(Lambda)](FRHICommandList&)
		{
			VOXEL_SCOPE_COUNTER("FVoxelTaskContext::Dispatch");

			if (!bIsExiting.Get())
			{
				FVoxelTaskScope Scope(*this);
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

void FVoxelTaskContext::DumpToLog()
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

void FVoxelTaskContext::LaunchTasks()
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

void FVoxelTaskContext::LaunchTask(TVoxelUniqueFunction<void()> Task)
{
	NumLaunchedTasks.Increment();

	UE::Tasks::Launch(
		TEXT("Voxel Task"),
		[this, Task = MoveTemp(Task)]
		{
			if (!bIsExiting.Get())
			{
				FVoxelTaskScope Scope(*this);
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

void FVoxelTaskContext::ProcessGameTasks(bool& bAnyTaskProcessed)
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

	FVoxelTaskScope Scope(*this);

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

const uint32 GVoxelTaskScopeTLS = FPlatformTLS::AllocTlsSlot();