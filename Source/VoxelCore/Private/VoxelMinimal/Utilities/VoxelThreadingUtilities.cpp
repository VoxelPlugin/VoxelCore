// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Async/Async.h"

TQueue<TVoxelUniqueFunction<void()>, EQueueMode::Mpsc> GVoxelGameThreadTaskQueue;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelGameThreadTaskTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		const double StartTime = FPlatformTime::Seconds();

		while (true)
		{
			if (FPlatformTime::Seconds() - StartTime > 0.1)
			{
				LOG_VOXEL(Warning, "Spent more than 100ms processing game thread tasks - throttling");
				return;
			}

			TVoxelUniqueFunction<void()> Lambda;
			if (!GVoxelGameThreadTaskQueue.Dequeue(Lambda))
			{
				return;
			}

			Lambda();
		}
	}
	//~ End FVoxelTicker Interface
};

VOXEL_RUN_ON_STARTUP_GAME()
{
	new FVoxelGameThreadTaskTicker();

	GOnVoxelModuleUnloaded_DoCleanup.AddLambda([]
	{
		FlushVoxelGameThreadTasks();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FlushVoxelGameThreadTasks()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	TVoxelUniqueFunction<void()> Lambda;
	while (GVoxelGameThreadTaskQueue.Dequeue(Lambda))
	{
		Lambda();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AsyncBackgroundTaskImpl(TVoxelUniqueFunction<void()> Lambda)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [Lambda = MoveTemp(Lambda)]
	{
		VOXEL_FUNCTION_COUNTER();
		Lambda();
	});
}

void AsyncThreadPoolTaskImpl(TVoxelUniqueFunction<void()> Lambda)
{
	Async(EAsyncExecution::ThreadPool, [Lambda = MoveTemp(Lambda)]
	{
		VOXEL_FUNCTION_COUNTER();
		Lambda();
	});
}

void RunOnGameThread_Async(TVoxelUniqueFunction<void()> Lambda)
{
	GVoxelGameThreadTaskQueue.Enqueue(MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

thread_local bool GVoxelAllowParallelTasks = false;
thread_local TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GVoxelTasks;

void FVoxelUtilities::Task(TVoxelUniqueFunction<void()> Lambda)
{
	if (!GVoxelAllowParallelTasks)
	{
		Lambda();
		return;
	}

	GVoxelTasks.Add(MoveTemp(Lambda));
}

void FVoxelUtilities::FlushTasks()
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelChunkedArray<TVoxelUniqueFunction<void()>> Tasks = MoveTemp(GVoxelTasks);
	check(GVoxelTasks.Num() == 0);

	if (GVoxelAllowParallelTasks)
	{
		ParallelFor(Tasks.Num(), [&](const int32 Index)
		{
			FVoxelTaskScope TaskScope(true);
			Tasks[Index]();
		});
	}
	else
	{
		for (const TVoxelUniqueFunction<void()>& Task : Tasks)
		{
			Task();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTaskScope::FVoxelTaskScope(const bool bAllowParallelTasks)
	: bPreviousAllowParallelTasks(GVoxelAllowParallelTasks)
{
	GVoxelAllowParallelTasks = bAllowParallelTasks;
}

FVoxelTaskScope::~FVoxelTaskScope()
{
	GVoxelAllowParallelTasks = bPreviousAllowParallelTasks;
}