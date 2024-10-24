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

	UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (ClampMin = 1))
	bool bEnablePerformanceMonitoring = true;

	// Number of frames to collect the average frame rate from
	UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (ClampMin = 2))
	int32 FramesToAverage = 128;

	// Minimum average FPS allowed in specified number of frames
	UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (ClampMin = 1))
	int32 MinFPS = 8;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};