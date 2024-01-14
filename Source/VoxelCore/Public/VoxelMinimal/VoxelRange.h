// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelRange.generated.h"

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelFloatRange
{
	GENERATED_BODY()

public:
	FVoxelFloatRange() = default;
	FVoxelFloatRange(const float Min, const float Max)
		: Min(Min)
		, Max(Max)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float Min = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float Max = 1;

public:
	FORCEINLINE float Interpolate(const float Alpha) const
	{
		if (!ensureVoxelSlow(Min <= Max))
		{
			return Min;
		}

		return FMath::Lerp(Min, Max, Alpha);
	}
};

USTRUCT(BlueprintType, DisplayName = "Voxel Integer Range")
struct VOXELCORE_API FVoxelInt32Range
{
	GENERATED_BODY()

public:
	FVoxelInt32Range() = default;
	FVoxelInt32Range(const int32 Min, const int32 Max)
		: Min(Min)
		, Max(Max)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Min = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Max = 1;

public:
	FORCEINLINE int32 Interpolate(const float Alpha) const
	{
		if (!ensureVoxelSlow(Min <= Max))
		{
			return Min;
		}

		return FMath::RoundToInt(FMath::Lerp(float(Min), float(Max), Alpha));
	}
};