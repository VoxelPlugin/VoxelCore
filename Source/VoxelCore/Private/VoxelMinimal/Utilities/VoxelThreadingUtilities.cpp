// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelThreadPool.h"
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

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelGameThreadTaskTicker)
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

void AsyncVoxelTaskImpl(TVoxelUniqueFunction<void()> Lambda)
{
	GVoxelThreadPool->AddTask(MoveTemp(Lambda));
}

void AsyncBackgroundTaskImpl(TVoxelUniqueFunction<void()> Lambda)
{
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [Lambda = MoveTemp(Lambda)]
	{
		VOXEL_FUNCTION_COUNTER();
		Lambda();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::RunOnGameThread_Async(TVoxelUniqueFunction<void()> Lambda)
{
	GVoxelGameThreadTaskQueue.Enqueue(MoveTemp(Lambda));
}