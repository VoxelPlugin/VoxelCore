// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGlobalTaskDispatcher.h"

const TSharedRef<IVoxelTaskDispatcher> GVoxelGlobalTaskDispatcher= MakeShared<FVoxelGlobalTaskDispatcher>();

void FVoxelGlobalTaskDispatcher::DispatchImpl(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
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
		UE::Tasks::Launch(
			TEXT("Voxel Global Task"),
			[this, Lambda = MoveTemp(Lambda)]
			{
				FVoxelTaskDispatcherScope Scope(*this);
				Lambda();
			},
			LowLevelTasks::ETaskPriority::BackgroundLow);
	}
	break;
	}
}

bool FVoxelGlobalTaskDispatcher::IsExiting() const
{
	return false;
}