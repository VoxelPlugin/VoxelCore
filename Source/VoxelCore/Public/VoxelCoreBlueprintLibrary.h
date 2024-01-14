// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelCoreBlueprintLibrary.generated.h"

UCLASS()
class VOXELCORE_API UVoxelCoreBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Use this after setting parameters on a MaterialInstanceDynamic used on voxel meshes
	// This will ensure the change is propagated properly
	UFUNCTION(BlueprintCallable, Category = "Voxel|Material")
	static void RefreshMaterialInstance(UMaterialInstanceDynamic* MaterialInstance);
};