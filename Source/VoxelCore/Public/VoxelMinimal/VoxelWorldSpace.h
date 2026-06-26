// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

enum class EVoxelWorldSpace : uint8
{
	// The current, rendered world. Shifted by world origin rebasing - viewport hits & preview actors live here.
	CurrentWorld,
	// World-origin independent space. Voxel generation, stamps and the sculpt data pipeline run here.
	Absolute
};
