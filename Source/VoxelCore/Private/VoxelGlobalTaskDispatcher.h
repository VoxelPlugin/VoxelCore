// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelThreadPool.h"
#include "VoxelTaskDispatcherInterface.h"

class FVoxelGlobalTaskDispatcher
	: public IVoxelTaskDispatcher
	, public IVoxelTaskExecutor
{
public:
	const bool bIsBackground;

	explicit FVoxelGlobalTaskDispatcher(bool bIsBackground);

	//~ Begin IVoxelTaskDispatcher Interface
	virtual void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda) override;

	virtual bool IsExiting() const override;
	//~ End IVoxelTaskDispatcher Interface

	//~ Begin IVoxelTaskExecutor Interface
	virtual bool TryExecuteTasks_AnyThread() override;
	virtual int32 NumTasks() const override;
	//~ End IVoxelTaskExecutor Interface

	int32 NumTasks_Actual() const
	{
		return AsyncTasks_RequiresLock.Num();
	}

private:
	FVoxelCriticalSection CriticalSection;
	TVoxelArray<TVoxelUniqueFunction<void()>> AsyncTasks_RequiresLock;
};

extern TSharedPtr<FVoxelGlobalTaskDispatcher> GVoxelGlobalForegroundTaskDispatcher;
extern TSharedPtr<FVoxelGlobalTaskDispatcher> GVoxelGlobalBackgroundTaskDispatcher;