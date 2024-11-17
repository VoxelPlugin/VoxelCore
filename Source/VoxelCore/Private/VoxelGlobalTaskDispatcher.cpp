// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGlobalTaskDispatcher.h"

TSharedPtr<FVoxelGlobalTaskDispatcher> GVoxelGlobalForegroundTaskDispatcher;
TSharedPtr<FVoxelGlobalTaskDispatcher> GVoxelGlobalBackgroundTaskDispatcher;

FVoxelGlobalTaskDispatcher::FVoxelGlobalTaskDispatcher(const bool bIsBackground)
	: bIsBackground(bIsBackground)
{
	GVoxelThreadPool->AddExecutor(this);
}

void FVoxelGlobalTaskDispatcher::Dispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
	switch (Thread)
	{
	default: VOXEL_ASSUME(false);
	case EVoxelFutureThread::AnyThread:
	{
		Lambda();
	}
	break;
	case EVoxelFutureThread::GameThread:
	{
		Voxel::GameTask_SkipDispatcher([this, Lambda = MoveTemp(Lambda)]
		{
			FVoxelTaskDispatcherScope Scope(*this);
			Lambda();
		});
	}
	break;
	case EVoxelFutureThread::RenderThread:
	{
		Voxel::RenderTask_SkipDispatcher([this, Lambda = MoveTemp(Lambda)]
		{
			FVoxelTaskDispatcherScope Scope(*this);
			Lambda();
		});
	}
	break;
	case EVoxelFutureThread::AsyncThread:
	{
		// Schedule on a voxel thread to avoid starving the task graph,
		// especially when processing PCG tasks

		VOXEL_SCOPE_LOCK(CriticalSection);

		AsyncTasks_RequiresLock.Add(MoveTemp(Lambda));

		if (AsyncTasks_RequiresLock.Num() < 32)
		{
			// Ensure a thread is awake if we don't have many tasks left
			// Over-conservative, but that's fine
			TriggerThreads();
		}
	}
	break;
	}
}

bool FVoxelGlobalTaskDispatcher::IsExiting() const
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelGlobalTaskDispatcher::TryExecuteTasks_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelTaskDispatcherScope Scope(*this);

	bool bAnyExecuted = false;
	while (true)
	{
		if (IsEngineExitRequested())
		{
			return false;
		}

		TVoxelUniqueFunction<void()> Lambda;
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			if (AsyncTasks_RequiresLock.Num() == 0)
			{
				return bAnyExecuted;
			}

			Lambda = AsyncTasks_RequiresLock.Pop();
		}

		Lambda();

		bAnyExecuted = true;

		if (bIsBackground)
		{
			// Only execute one task at a time if we're low priority
			return bAnyExecuted;
		}
	}
}

int32 FVoxelGlobalTaskDispatcher::NumTasks() const
{
	// Don't report tasks, too spammy
	return 0; // AsyncTasks_RequiresLock.Num();
}