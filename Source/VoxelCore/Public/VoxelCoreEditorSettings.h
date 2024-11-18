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
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (InlineEditConditionToggle))
	bool bUseNumberOfThreadsInEditor = false;

	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (EditCondition = "bUseNumberOfThreadsInEditor", ClampMin = 1))
	int32 NumberOfThreadsInEditor = 2;
#endif
};