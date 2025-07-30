// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelShaderHook.h"
#include "VoxelDeveloperSettings.h"
#include "VoxelShaderHooksManager.generated.h"

UCLASS(config=EditorPerProjectUserSettings)
class VOXELCORE_API UVoxelShaderHooksSettings : public UVoxelDeveloperSettings
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
	UPROPERTY(Config)
	TSet<FName> DisabledHooks;
#endif
};

#if WITH_EDITOR
class VOXELCORE_API FVoxelShaderHooksManager
{
public:
	void RegisterHook(FVoxelShaderHookGroup& Group);

	TVoxelArray<FVoxelShaderHookGroup*> Hooks;
	TVoxelMap<FGuid, const FVoxelShaderHook*> GuidToHook;
};

extern VOXELCORE_API FVoxelShaderHooksManager* GVoxelShaderHooksManager;
#endif