// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelIntInterval.generated.h"

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelIntInterval
{
	GENERATED_BODY()

	// Included
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Min = 0;

	// Excluded
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Max = 0;

	static const FVoxelIntInterval Infinite;
	static const FVoxelIntInterval InvertedInfinite;

	FVoxelIntInterval() = default;

	FORCEINLINE FVoxelIntInterval(const int32 Min, const int32 Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min <= Max);
	}

	static FVoxelIntInterval FromValues(TConstVoxelArrayView<int32> Values);

	FORCEINLINE int32 Size() const
	{
		return Max - Min;
	}
	FORCEINLINE float GetCenter() const
	{
		return (Min + Max) / 2.f;
	}

	FString ToString() const;

	FORCEINLINE bool IsValid() const
	{
		return Min <= Max;
	}
	FORCEINLINE bool IsValidAndNotEmpty() const
	{
		return
			IsValid() &&
			*this != FVoxelIntInterval();
	}

	FORCEINLINE bool Contains(const int32 Value) const
	{
		return Min <= Value && Value <= Max;
	}
	FORCEINLINE bool IsInfinite() const
	{
		// Not exactly accurate, but should be safe
		const int32 InfiniteMin = MIN_int32 / 2;
		const int32 InfiniteMax = MAX_int32 / 2;
		return
			Min < InfiniteMin ||
			Max > InfiniteMax;
	}

	FORCEINLINE bool Contains(const FVoxelIntInterval& Other) const
	{
		return Min <= Other.Min && Other.Max <= Max;
	}

	FORCEINLINE bool Intersects(const FVoxelIntInterval& Other) const
	{
		return Min <= Other.Max && Other.Min <= Max;
	}

	FORCEINLINE FVoxelIntInterval IntersectWith(const FVoxelIntInterval& Other) const
	{
		const int32 NewMin = FMath::Max(Min, Other.Min);
		const int32 NewMax = FMath::Min(Max, Other.Max);

		if (NewMin > NewMax)
		{
			return {};
		}

		return FVoxelIntInterval(NewMin, NewMax);
	}
	FORCEINLINE FVoxelIntInterval UnionWith(const FVoxelIntInterval& Other) const
	{
		return FVoxelIntInterval(
			FMath::Min(Min, Other.Min),
			FMath::Max(Max, Other.Max));
	}

	FORCEINLINE FVoxelIntInterval Scale(const int32 S) const
	{
		const int32 A = Min * S;
		const int32 B = Max * S;

		return FVoxelIntInterval(
			FMath::Min(A, B),
			FMath::Max(A, B));
	}

	FORCEINLINE FVoxelIntInterval Extend(const int32 Amount) const
	{
		return { Min - Amount, Max + Amount };
	}

	FORCEINLINE FVoxelIntInterval ShiftBy(const int32 Offset) const
	{
		return FVoxelIntInterval(Min + Offset, Max + Offset);
	}

	FORCEINLINE FVoxelIntInterval& operator*=(const int32 S)
	{
		*this = Scale(S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelIntInterval& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}

	FORCEINLINE FVoxelIntInterval& operator+=(const FVoxelIntInterval& Other)
	{
		Min = FMath::Min(Min, Other.Min);
		Max = FMath::Max(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelIntInterval& operator+=(const int32 Value)
	{
		Min = FMath::Min(Min, Value);
		Max = FMath::Max(Max, Value);
		return *this;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelIntInterval& Box)
{
	return FVoxelUtilities::MurmurHash(Box);
}

FORCEINLINE FVoxelIntInterval operator*(const FVoxelIntInterval& Box, const int32 Scale)
{
	return FVoxelIntInterval(Box) *= Scale;
}
FORCEINLINE FVoxelIntInterval operator*(const int32 Scale, const FVoxelIntInterval& Box)
{
	return FVoxelIntInterval(Box) *= Scale;
}

FORCEINLINE FVoxelIntInterval operator+(const FVoxelIntInterval& Box, const FVoxelIntInterval& Other)
{
	return FVoxelIntInterval(Box) += Other;
}
FORCEINLINE FVoxelIntInterval operator+(const FVoxelIntInterval& Box, const int32 Point)
{
	return FVoxelIntInterval(Box) += Point;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelIntInterval& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}