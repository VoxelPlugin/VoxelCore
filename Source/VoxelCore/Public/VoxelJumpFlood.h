// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelJumpFlood
{
public:
	static void JumpFlood2D(
		const FIntPoint& Size,
		TVoxelArrayView<FIntPoint> InOutClosestPosition);

private:
	static void JumpFlood2DImpl(
		const FIntPoint& Size,
		TConstVoxelArrayView<FIntPoint> InData,
		TVoxelArrayView<FIntPoint> OutData,
		int32 Step);
};