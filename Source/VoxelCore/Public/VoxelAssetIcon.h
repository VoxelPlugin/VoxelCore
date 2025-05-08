// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelAssetIcon.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelAssetIcon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Asset Icon")
	bool bCustomIcon = false;

	UPROPERTY(EditAnywhere, Category = "Asset Icon", meta = (EditCondition = "bCustomIcon"))
	FLinearColor Color = FLinearColor(0.038462f, 0.038462f, 0.038462f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Asset Icon", meta = (EditCondition = "bCustomIcon"))
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, Category = "Asset Icon", meta = (EditCondition = "bCustomIcon"))
	FLinearColor IconColor = FLinearColor::White;
};