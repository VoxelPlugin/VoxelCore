// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FMaterialShaderParameters;

struct VOXELCORE_API FVoxelMaterialUsage
{
	static void CheckMaterial(UMaterialInterface* Material);
	static bool ShouldCompilePermutation(const FMaterialShaderParameters& MaterialParameters);
};

#define bUsedWithVoxelPlugin bUsedWithLidarPointCloud