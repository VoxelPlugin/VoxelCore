// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Async/Async.h"

class FVoxelGameThreadTaskTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		FlushVoxelGameThreadTasks();
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

TQueue<TVoxelUniqueFunction<void()>, EQueueMode::Mpsc> GVoxelGameThreadTaskQueue;

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

void AsyncVoxelTask(TVoxelUniqueFunction<void()>&& Lambda)
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

void FVoxelUtilities::RunOnGameThread_Async(TVoxelUniqueFunction<void()>&& Lambda)
{
	GVoxelGameThreadTaskQueue.Enqueue(MoveTemp(Lambda));
}