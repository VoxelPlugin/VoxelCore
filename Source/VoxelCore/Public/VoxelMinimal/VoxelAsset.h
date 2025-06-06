// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelAsset.generated.h"

UCLASS()
class VOXELCORE_API UVoxelAsset : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UObject Interface
};