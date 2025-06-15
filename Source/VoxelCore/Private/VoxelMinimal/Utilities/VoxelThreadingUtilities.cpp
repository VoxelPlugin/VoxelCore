// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"
#include "Async/Async.h"

FVoxelShouldCancel::FVoxelShouldCancel()
	: ShouldCancelTasks(FVoxelTaskScope::GetContext().GetShouldCancelTasksRef())
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSimpleMulticastDelegate Voxel::OnForceTick;

void Voxel::ForceTick()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	OnForceTick.Broadcast();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool Voxel::ShouldCancel()
{
	return FVoxelTaskScope::GetContext().IsCancellingTasks();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture Voxel::ExecuteSynchronously_Impl(const TFunctionRef<FVoxelFuture()> Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelTaskScope Scope(*GVoxelSynchronousTaskContext);

	const FVoxelFuture Future = Lambda();

	GVoxelSynchronousTaskContext->FlushTasksUntil([&]
	{
		return Future.IsComplete();
	});

	ensure(GVoxelSynchronousTaskContext->IsComplete());

	check(Future.IsComplete());
	return Future;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

FVoxelParallelTaskScope::~FVoxelParallelTaskScope()
{
	FlushTasks();
}

void FVoxelParallelTaskScope::AddTask(TVoxelUniqueFunction<void()> Lambda)
{
	if (GVoxelNoAsync)
	{
		Lambda();
		return;
	}

	Tasks.Enqueue(UE::Tasks::Launch(
		TEXT("Voxel Parallel Task"),
		MoveTemp(Lambda),
		IsInGameThread()
		? LowLevelTasks::ETaskPriority::High
		: LowLevelTasks::ETaskPriority::BackgroundLow));
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