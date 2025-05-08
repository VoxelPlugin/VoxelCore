// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelInterval.generated.h"

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelInterval
{
	GENERATED_BODY()

	// Included
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	double Min = 0;

	// Included
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	double Max = 0;

	static const FVoxelInterval Infinite;
	static const FVoxelInterval InvertedInfinite;

	FVoxelInterval() = default;

	FORCEINLINE FVoxelInterval(const double Min, const double Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min <= Max);
	}
	FORCEINLINE FVoxelInterval(const FFloatInterval& Other)
		: Min(Other.Min)
		, Max(Other.Max)
	{
		ensureVoxelSlow(Min <= Max);
	}
	FORCEINLINE FVoxelInterval(const FDoubleInterval& Other)
		: Min(Other.Min)
		, Max(Other.Max)
	{
		ensureVoxelSlow(Min <= Max);
	}

	static FVoxelInterval FromValues(TConstVoxelArrayView<float> Values);
	static FVoxelInterval FromValues(TConstVoxelArrayView<double> Values);

	FORCEINLINE double Size() const
	{
		return Max - Min;
	}
	FORCEINLINE double GetCenter() const
	{
		return (Min + Max) / 2;
	}
	FORCEINLINE double GetExtent() const
	{
		return (Max - Min) / 2;
	}

	FString ToString() const;

	FORCEINLINE bool IsValid() const
	{
		return
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Min)) &&
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Max)) &&
			Min <= Max;
	}
	FORCEINLINE bool IsValidAndNotEmpty() const
	{
		return
			IsValid() &&
			*this != FVoxelInterval();
	}

	FORCEINLINE bool Contains(const double Value) const
	{
		return Min <= Value && Value <= Max;
	}
	FORCEINLINE bool IsInfinite() const
	{
		return Contains(FVoxelInterval(-1e20, 1e20));
	}

	FORCEINLINE bool Contains(const FVoxelInterval& Other) const
	{
		return Min <= Other.Min && Other.Max <= Max;
	}

	FORCEINLINE bool Intersects(const FVoxelInterval& Other) const
	{
		return Min <= Other.Max && Other.Min <= Max;
	}

	FORCEINLINE FVoxelInterval IntersectWith(const FVoxelInterval& Other) const
	{
		const double NewMin = FMath::Max(Min, Other.Min);
		const double NewMax = FMath::Min(Max, Other.Max);

		if (NewMin > NewMax)
		{
			return {};
		}

		return FVoxelInterval(NewMin, NewMax);
	}
	FORCEINLINE FVoxelInterval UnionWith(const FVoxelInterval& Other) const
	{
		return FVoxelInterval(
			FMath::Min(Min, Other.Min),
			FMath::Max(Max, Other.Max));
	}

	FORCEINLINE FVoxelInterval Scale(const double S) const
	{
		const double A = Min * S;
		const double B = Max * S;

		return FVoxelInterval(
			FMath::Min(A, B),
			FMath::Max(A, B));
	}

	FORCEINLINE FVoxelInterval Extend(const double Amount) const
	{
		FVoxelInterval Result;
		// Skip constructor checks
		Result.Min = Min - Amount;
		Result.Max = Max + Amount;

		if (Result.Min > Result.Max)
		{
			const double Middle = (Result.Min + Result.Max) / 2;
			Result.Min = Middle;
			Result.Max = Middle;
		}

		return Result;
	}

	FORCEINLINE FVoxelInterval ShiftBy(const double Offset) const
	{
		return FVoxelInterval(Min + Offset, Max + Offset);
	}

	FORCEINLINE FVoxelInterval& operator*=(const double S)
	{
		*this = Scale(S);
		return *this;
	}
	FORCEINLINE FVoxelInterval& operator/=(const double S)
	{
		*this = Scale(1. / S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelInterval& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}

	FORCEINLINE FVoxelInterval& operator+=(const FVoxelInterval& Other)
	{
		Min = FMath::Min(Min, Other.Min);
		Max = FMath::Max(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelInterval& operator+=(const double Value)
	{
		Min = FMath::Min(Min, Value);
		Max = FMath::Max(Max, Value);
		return *this;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelInterval& Box)
{
	return FVoxelUtilities::MurmurHash(Box);
}

FORCEINLINE FVoxelInterval operator*(const FVoxelInterval& Box, const double Scale)
{
	return FVoxelInterval(Box) *= Scale;
}
FORCEINLINE FVoxelInterval operator*(const double Scale, const FVoxelInterval& Box)
{
	return FVoxelInterval(Box) *= Scale;
}

FORCEINLINE FVoxelInterval operator/(const FVoxelInterval& Box, const double Scale)
{
	return FVoxelInterval(Box) /= Scale;
}

FORCEINLINE FVoxelInterval operator+(const FVoxelInterval& Box, const FVoxelInterval& Other)
{
	return FVoxelInterval(Box) += Other;
}
FORCEINLINE FVoxelInterval operator+(const FVoxelInterval& Box, const double Point)
{
	return FVoxelInterval(Box) += Point;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelInterval& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}