// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMaterialUsage.h"
#include "MaterialShared.h"
#include "Materials/Material.h"

#if WITH_EDITOR
VOXEL_RUN_ON_STARTUP_EDITOR()
{
	FProperty& Property = FindFPropertyChecked(UMaterial, bUsedWithLidarPointCloud);
	Property.SetMetaData("DisplayName", "Use with Voxel Plugin or Lidar");

	Property.SetMetaData("Tooltip",
		"Indicates that the material and its instances can be use with Voxel Plugin & with LiDAR Point Clouds.\n"
		"This will result in the shaders required to support Voxel Plugin and LiDAR being compiled which will increase shader compile time and memory usage.");
}
#endif

void FVoxelMaterialUsage::CheckMaterial(UMaterialInterface* Material)
{
	ensure(IsInGameThread());

	if (!Material)
	{
		return;
	}

	ensureVoxelSlow(Material->CheckMaterialUsage(MATUSAGE_LidarPointCloud));
}

bool FVoxelMaterialUsage::ShouldCompilePermutation(const FMaterialShaderParameters& MaterialParameters)
{
	return
		MaterialParameters.bIsSpecialEngineMaterial ||
		MaterialParameters.bIsUsedWithLidarPointCloud;
}