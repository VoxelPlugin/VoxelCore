// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDeveloperSettings.h"
#include "VoxelCoreSettings.generated.h"

UCLASS(config = Engine, DefaultConfig, meta = (DisplayName = "Voxel Plugin Core"))
class VOXELCORE_API UVoxelCoreSettings : public UVoxelDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoxelCoreSettings()
	{
		SectionName = "Voxel Plugin Core";
	}

	// Number of threads allocated for the voxel background processing. Setting it too high may impact performance
	// Can be set using voxel.NumThreads
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (ClampMin = 1, ConsoleVariable = "voxel.NumThreads"))
	int32 NumberOfThreads = 0;
};