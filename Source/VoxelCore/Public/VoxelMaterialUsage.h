// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "MeshMaterialShader.h"
#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelMaterialUsage
{
	static void CheckMaterial(UMaterialInterface* Material);
	static bool ShouldCompilePermutation(const FMaterialShaderParameters& MaterialParameters);
};