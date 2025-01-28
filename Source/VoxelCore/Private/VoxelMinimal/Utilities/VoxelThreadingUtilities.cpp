// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Async/Async.h"

TMulticastDelegate<void(bool& bAnyTaskProcessed)> Voxel::OnFlushGameTasks;

void Voxel::FlushGameTasks()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	while (true)
	{
		bool bAnyTaskProcessed = false;
		OnFlushGameTasks.Broadcast(bAnyTaskProcessed);

		if (!bAnyTaskProcessed)
		{
			break;
		}
	}
}

void Voxel::AsyncTask_ThreadPool_Impl(TVoxelUniqueFunction<void()> Lambda)
{
	Async(EAsyncExecution::ThreadPool, [Lambda = MoveTemp(Lambda)]
	{
		VOXEL_FUNCTION_COUNTER();
		Lambda();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

thread_local bool GVoxelAllowParallelTasks = false;
thread_local TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GVoxelTasks;

FVoxelParallelTaskScope::~FVoxelParallelTaskScope()
{
	FlushTasks();
}

void FVoxelParallelTaskScope::AddTask(TVoxelUniqueFunction<void()> Lambda)
{
	Tasks.Enqueue(UE::Tasks::Launch(
		TEXT("Voxel Parallel Task"),
		MoveTemp(Lambda),
		LowLevelTasks::ETaskPriority::BackgroundLow));
}

void FVoxelParallelTaskScope::FlushTasks()
{
	VOXEL_FUNCTION_COUNTER();

	UE::Tasks::TTask<void> Task;
	while (Tasks.Dequeue(Task))
	{
		verify(Task.Wait());
	}

	check(Tasks.IsEmpty());
}