// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDeveloperSettings.h"
#include "VoxelCoreEditorSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings)
class VOXELCORE_API UVoxelCoreEditorSettings : public UVoxelDeveloperSettings
{
	GENERATED_BODY()

public:
	//~ Begin UVoxelDeveloperSettings Interface
	virtual FName GetContainerName() const override
	{
		return "Editor";
	}
	//~ End UVoxelDeveloperSettings Interface

#if WITH_EDITORONLY_DATA
	// If true, will use the game settings (ie, voxel.NumThreads) as the number of threads
	// It's recommended to keep this false for faster iteration in editor
	// PIE will always use the game settings
	UPROPERTY(Config, EditAnywhere, Category = "Config")
	bool bUseGameThreadingSettingsInEditor = false;

	// The number of threads will be set to NumberOfCores - 2
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (EditCondition = "!bUseGameThreadingSettingsInEditor"))
	bool bAutomaticallyScaleNumberOfThreads = true;

	// Manual override for the number of threads to use in editor
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (EditCondition = "!bUseGameThreadingSettingsInEditor && !bAutomaticallyScaleNumberOfThreads", ClampMin = 1))
	int32 NumberOfThreadsInEditor = 2;
#endif
};