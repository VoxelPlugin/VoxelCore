// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelCoreBlueprintLibrary.h"

void UVoxelCoreBlueprintLibrary::RefreshMaterialInstance(UMaterialInstanceDynamic* MaterialInstance)
{
	VOXEL_FUNCTION_COUNTER();

	if (!MaterialInstance)
	{
		VOXEL_MESSAGE(Error, "MaterialInstance is null");
		return;
	}

	FVoxelMaterialRef::RefreshInstance(*MaterialInstance);
}