// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskContext.h"
#include "Async/Async.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelNoAsync, false,
	"voxel.NoAsync",
	"If true, will run all voxel tasks on the game thread. Useful when debugging.");

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelTrackAllPromisesCallstacks, false,
	"voxel.TrackAllPromisesCallstacks",
	"Enable voxel promise callstack tracking, to debug when promises where created");

VOXEL_RUN_ON_STARTUP_GAME()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("NoVoxelAsync")))
	{
		GVoxelNoAsync = true;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

thread_local int32 GVoxelIsWaitingOnFuture = 0;
FVoxelTaskContext* GVoxelGlobalTaskContext = nullptr;
FVoxelTaskContext* GVoxelSynchronousTaskContext = nullptr;

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelTaskContext);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelTaskContextArray
{
	FVoxelSharedCriticalSection CriticalSection;
	TVoxelSparseArray<FVoxelTaskContext*> Contexts_RequiresLock;
	FVoxelCounter32 SerialCounter;
};
FVoxelTaskContextArray* GVoxelTaskContextArray = new FVoxelTaskContextArray();

class FVoxelTaskContextTicker : public FVoxelSingleton
{
public:
	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		GVoxelGlobalTaskContext = new FVoxelTaskContext("GlobalTaskContext");
		GVoxelSynchronousTaskContext = new FVoxelTaskContext("ExecuteSynchronously");

		GVoxelSynchronousTaskContext->bSynchronous = true;

		Voxel::OnFlushGameTasks.AddLambda([this](bool& bAnyTaskProcessed)
		{
			ProcessGameTasks(bAnyTaskProcessed);
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

		TVoxelArray<FVoxelTaskContextWeakRef> WeakRefs;
		WeakRefs.Reserve(GVoxelTaskContextArray->Contexts_RequiresLock.Num());
		{
			VOXEL_SCOPE_READ_LOCK(GVoxelTaskContextArray->CriticalSection);

			for (FVoxelTaskContext* Context : GVoxelTaskContextArray->Contexts_RequiresLock)
			{
				WeakRefs.Emplace(*Context);
			}
		}

		for (const FVoxelTaskContextWeakRef& WeakRef : WeakRefs)
		{
			TUniquePtr<FVoxelTaskContextStrongRef> StrongRef = WeakRef.Pin();
			if (!StrongRef)
			{
				continue;
			}

			TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GameTasksToDelete;

			StrongRef->Context.ProcessGameTasks(bAnyTaskProcessed, GameTasksToDelete);
			StrongRef.Reset();

			// Delete the tasks AFTER the strong ref is released, as one of the task could be the last thing keeping the task context alive
			// (in which case we get into an infinite loop if we are still holding a strong ref to it)
			GameTasksToDelete.Empty();
		}
	}
};
FVoxelTaskContextTicker* GVoxelTaskContextTicker = new FVoxelTaskContextTicker();

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

	VOXEL_SCOPE_READ_LOCK(GVoxelTaskContextArray->CriticalSection);

	if (!GVoxelTaskContextArray->Contexts_RequiresLock.IsAllocated(Index))
	{
		return {};
	}

	FVoxelTaskContext* Context = GVoxelTaskContextArray->Contexts_RequiresLock[Index];
	checkVoxelSlow(Context);
	checkVoxelSlow(Context->SelfWeakRef.Index == Index);

	if (Context->ShouldCancelTasks.Get() ||
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

FVoxelTaskContext::FVoxelTaskContext(const FName Name)
	: Name(Name)
{
	VOXEL_SCOPE_WRITE_LOCK(GVoxelTaskContextArray->CriticalSection);

	SelfWeakRef.Index = GVoxelTaskContextArray->Contexts_RequiresLock.Add(this);
	SelfWeakRef.Serial = GVoxelTaskContextArray->SerialCounter.Increment_ReturnNew();
}

TSharedRef<FVoxelTaskContext> FVoxelTaskContext::Create(const FName Name, const int32 MaxBackgroundTasks)
{
	FVoxelTaskContext* Context = new FVoxelTaskContext(Name);
	Context->MaxBackgroundTasks = MaxBackgroundTasks;

	if (GVoxelTrackAllPromisesCallstacks)
	{
		Context->bTrackPromisesCallstacks = true;
	}

	return MakeShareable_CustomDestructor(Context, [=]
	{
		checkVoxelSlow(Context != GVoxelGlobalTaskContext);

		Context->CancelTasks();

		const auto ScheduleDelete = [Context]
		{
			// Make sure to delete the task context in the global task context, see FVoxelTaskContext::~FVoxelTaskContext
			GVoxelGlobalTaskContext->Dispatch(EVoxelFutureThread::AsyncThread, [Context]
			{
				delete Context;
			});
		};

		if (Context->NumRenderTasks.Get() > 0)
		{
			// Wait for the render tasks to be done before deleting to avoid a stall
			ENQUEUE_RENDER_COMMAND(FVoxelTaskContext_Destroy)([=](FRHICommandList&)
			{
				ScheduleDelete();
			});
		}
		else
		{
			ScheduleDelete();
		}
	});
}

FVoxelTaskContext::~FVoxelTaskContext()
{
	VOXEL_FUNCTION_COUNTER();

	// Can't delete within ourselves, would loop forever
	check(this == GVoxelGlobalTaskContext || &FVoxelTaskScope::GetContext() != this);

	CancelTasks();

	while (true)
	{
		FlushTasksUntil([&]
		{
			return
				NumStrongRefs.Get() == 0 &&
				NumPendingTasks.Get() == 0;
		});

		if (NumStrongRefs.Get() == 0 &&
			NumPendingTasks.Get() == 0)
		{
			if (GVoxelTaskContextArray->CriticalSection.TryWriteLock())
			{
				if (NumStrongRefs.Get() == 0 &&
					NumPendingTasks.Get() == 0)
				{
					break;
				}

				GVoxelTaskContextArray->CriticalSection.WriteUnlock();
			}
		}

		FVoxelUtilities::Yield();
	}
	check(NumStrongRefs.Get() == 0);
	check(NumPendingTasks.Get() == 0);
	check(NumLaunchedTasks.Get() == 0);
	check(NumRenderTasks.Get() == 0);

	check(GVoxelTaskContextArray->Contexts_RequiresLock[SelfWeakRef.Index] == this);
	GVoxelTaskContextArray->Contexts_RequiresLock.RemoveAt(SelfWeakRef.Index);

	GVoxelTaskContextArray->CriticalSection.WriteUnlock();
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

	if (ShouldCancelTasks.Get())
	{
		return;
	}

	if (LambdaWrapper)
	{
		Lambda = LambdaWrapper(MoveTemp(Lambda));
	}

	if (bComputeTotalTime)
	{
		Lambda = [Lambda = MoveTemp(Lambda), this]
		{
			const double StartTime = FPlatformTime::Seconds();
			Lambda();
			const double EndTime = FPlatformTime::Seconds();

			TotalTime.Add(EndTime - StartTime);
		};
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
		NumRenderTasks.Increment();

		// One ENQUEUE_RENDER_COMMAND per call, otherwise the command ordering can be incorrect
		ENQUEUE_RENDER_COMMAND(FVoxelTaskContext)([this, Lambda = MoveTemp(Lambda)](FRHICommandList&)
		{
			VOXEL_SCOPE_COUNTER("FVoxelTaskContext::Dispatch");

			if (!ShouldCancelTasks.Get())
			{
				FVoxelTaskScope Scope(*this);
				Lambda();
			}

			NumPendingTasks.Decrement();
			NumRenderTasks.Decrement();
		});
	}
	break;
	case EVoxelFutureThread::AsyncThread:
	{
		if (bSynchronous ||
			GVoxelIsWaitingOnFuture)
		{
			FVoxelTaskScope Scope(*this);
			Lambda();
			return;
		}

		NumPendingTasks.Increment();

		if (NumLaunchedTasks.Get() < MaxBackgroundTasks)
		{
			LaunchTask(MoveTemp(Lambda));
			return;
		}

		{
			VOXEL_SCOPE_LOCK(AsyncTasksCriticalSection);
			AsyncTasks_RequiresLock.Add(MoveTemp(Lambda));
		}

		if (NumLaunchedTasks.Get() < MaxBackgroundTasks)
		{
			LaunchTasks();
		}
	}
	break;
	}
}

void FVoxelTaskContext::CancelTasks()
{
	VOXEL_FUNCTION_COUNTER();

	ShouldCancelTasks.Set(true);

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
}

void FVoxelTaskContext::DumpToLog() const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	LOG_VOXEL(Log, "Queued game tasks: %d", GameTasks_RequiresLock.Num());
	LOG_VOXEL(Log, "Queued async tasks: %d", AsyncTasks_RequiresLock.Num());
	LOG_VOXEL(Log, "Launched async tasks: %d", NumLaunchedTasks.Get());

	LOG_VOXEL(Log, "Num promises: %d", GetNumPromises());
	LOG_VOXEL(Log, "Num pending tasks: %d", NumPendingTasks.Get());

	TVoxelMap<FVoxelStackFrames, int32> StackFramesToCount;
	StackFramesToCount.Reserve(PromisesToKeepAlive_RequiresLock.Num());

	for (const auto& It : PromiseStateToStackFrames_RequiresLock)
	{
		StackFramesToCount.FindOrAdd(It.Value)++;
	}

	StackFramesToCount.ValueSort([](const int32 A, const int32 B)
	{
		return A > B;
	});

	for (const auto& It : StackFramesToCount)
	{
		LOG_VOXEL(Log, "x%d:", It.Value);

		for (const FString& Line : FVoxelUtilities::StackFramesToString_WithStats(It.Key))
		{
			LOG_VOXEL(Log, "\t%s", *Line);
		}
	}
}

void FVoxelTaskContext::FlushAllTasks() const
{
	VOXEL_FUNCTION_COUNTER();

	FlushTasksUntil([&]
	{
		if (ShouldCancelTasks.Get())
		{
			// NumPromises will never be zero when cancelling
			return
				NumStrongRefs.Get() == 0 &&
				NumPendingTasks.Get() == 0;
		}
		else
		{
			return
				NumPromises.Get() == 0 &&
				NumPendingTasks.Get() == 0;
		}
	});
}

void FVoxelTaskContext::FlushTasksUntil(const TFunctionRef<bool()> Condition) const
{
	VOXEL_FUNCTION_COUNTER();

	double LastLogTime = FPlatformTime::Seconds();

	// if NumStrongRefs > 0, we need to wait for the promise who pinned us to complete

	while (!Condition())
	{
		if (IsInGameThread())
		{
			Voxel::FlushGameTasks();

			// ProcessGameTasks holds a strong ref, if this happens we are stuck
			checkf(!bIsProcessingGameTasks, TEXT("FVoxelTaskContext deleted during ProcessGameTasks"));
		}

		if (Condition())
		{
			return;
		}

		if (IsInGameThread() &&
			NumRenderTasks.Get() > 0)
		{
			// Only do this if really necessary
			FlushRenderingCommands();
		}

		if (Condition())
		{
			return;
		}

		if (FPlatformTime::Seconds() - LastLogTime > 1)
		{
			LastLogTime = FPlatformTime::Seconds();

			LOG_VOXEL(Log, "FlushTasks: waiting for %d tasks (%d promises)",
				NumPendingTasks.Get(),
				NumPromises.Get());
		}

		FVoxelUtilities::Yield();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTaskContext::LaunchTasks()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(AsyncTasksCriticalSection);
	checkStatic(2 * FTaskArray::NumPerChunk == MaxLaunchedTasks);

	while (
		AsyncTasks_RequiresLock.Num() > 0 &&
		NumLaunchedTasks.Get() < MaxBackgroundTasks)
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

	auto Lambda = [this, Task = MoveTemp(Task)]
	{
		if (!ShouldCancelTasks.Get())
		{
			FVoxelTaskScope Scope(*this);
			Task();
		}

		if (NumLaunchedTasks.Decrement_ReturnNew() < MaxBackgroundTasks)
		{
			LaunchTasks();
		}

		// Decrement allows us to be deleted, make sure to do it last
		NumPendingTasks.Decrement();
	};

	if (GVoxelNoAsync)
	{
		AsyncTask(
			ENamedThreads::GameThread,
			MoveTemp(Lambda));

		return;
	}

	UE::Tasks::Launch(
		TEXT("Voxel Task"),
		MoveTemp(Lambda),
		LowLevelTasks::ETaskPriority::BackgroundLow);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTaskContext::ProcessGameTasks(
	bool& bAnyTaskProcessed,
	TVoxelChunkedArray<TVoxelUniqueFunction<void()>>& OutGameTasksToDelete)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	check(!bIsProcessingGameTasks);
	bIsProcessingGameTasks = true;
	ON_SCOPE_EXIT
	{
		check(bIsProcessingGameTasks);
		bIsProcessingGameTasks = false;
	};

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
		if (!ShouldCancelTasks.Get())
		{
			Task();
		}
		NumPendingTasks.Decrement();
	}

	OutGameTasksToDelete = MoveTemp(GameTasks);
}

void FVoxelTaskContext::TrackPromise(const FVoxelPromiseState& PromiseState)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	PromiseStateToStackFrames_RequiresLock.Add_EnsureNew(&PromiseState, FVoxelUtilities::GetStackFrames_WithStats(5));
}

void FVoxelTaskContext::UntrackPromise(const FVoxelPromiseState& PromiseState)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(PromiseStateToStackFrames_RequiresLock.Remove(&PromiseState));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const uint32 GVoxelTaskScopeTLS = FPlatformTLS::AllocTlsSlot();
