// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTaskDispatcherInterface.h"

class FVoxelGlobalTaskDispatcher : public IVoxelTaskDispatcher
{
public:
	FVoxelGlobalTaskDispatcher()
		: IVoxelTaskDispatcher(false)
	{
	}

	//~ Begin IVoxelTaskDispatcher Interface
	virtual void DispatchImpl(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda) override;

	virtual bool IsExiting() const override;
	//~ End IVoxelTaskDispatcher Interface
};