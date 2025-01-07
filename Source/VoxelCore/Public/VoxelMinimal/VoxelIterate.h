// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

enum class EVoxelIterate : uint8
{
	Continue,
	Stop
};

enum class EVoxelIterateTree : uint8
{
	Continue,
	SkipChildren,
	Stop
};