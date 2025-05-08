// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

struct FVoxelTickerData;

class VOXELCORE_API FVoxelTicker
{
public:
	FVoxelTicker();
	virtual ~FVoxelTicker();

	virtual void Tick() = 0;

private:
	FVoxelTickerData* TickerData = nullptr;
};