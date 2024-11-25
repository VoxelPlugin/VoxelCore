// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"
#include "VoxelBox.generated.h"

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelBox
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	FVector Min = FVector3d(ForceInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	FVector Max = FVector3d(ForceInit);

	static const FVoxelBox Infinite;
	static const FVoxelBox InvertedInfinite;

	FVoxelBox() = default;

	FORCEINLINE FVoxelBox(const FVector3d& Min, const FVector3d& Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min.X <= Max.X);
		ensureVoxelSlow(Min.Y <= Max.Y);
		ensureVoxelSlow(Min.Z <= Max.Z);
	}
	FORCEINLINE explicit FVoxelBox(const double Min, const FVector3d& Max)
		: FVoxelBox(FVector3d(Min), Max)
	{
	}
	FORCEINLINE explicit FVoxelBox(const FVector3d& Min, const double Max)
		: FVoxelBox(Min, FVector3d(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox(const FVector3f& Min, const FVector3f& Max)
		: FVoxelBox(FVector3d(Min), FVector3d(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox(const FIntVector& Min, const FIntVector& Max)
		: FVoxelBox(FVector3d(Min), FVector3d(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox(const double Min, const double Max)
		: FVoxelBox(FVector3d(Min), FVector3d(Max))
	{
	}

	FORCEINLINE explicit FVoxelBox(const FVector3f& Position)
		: FVoxelBox(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelBox(const FVector3d& Position)
		: FVoxelBox(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelBox(const FIntVector& Position)
		: FVoxelBox(Position, Position)
	{
	}

	FORCEINLINE explicit FVoxelBox(const FBox3f& Box)
		: FVoxelBox(Box.Min, Box.Max)
	{
		ensureVoxelSlow(Box.IsValid);
	}
	FORCEINLINE explicit FVoxelBox(const FBox3d& Box)
		: FVoxelBox(Box.Min, Box.Max)
	{
		ensureVoxelSlow(Box.IsValid);
	}

	static FVoxelBox FromPositions(TConstVoxelArrayView<FIntVector> Positions);
	static FVoxelBox FromPositions(TConstVoxelArrayView<FVector3f> Positions);
	static FVoxelBox FromPositions(TConstVoxelArrayView<FVector3d> Positions);
	static FVoxelBox FromPositions(TConstVoxelArrayView<FVector4f> Positions);

	static FVoxelBox FromPositions(
		TConstVoxelArrayView<float> PositionX,
		TConstVoxelArrayView<float> PositionY,
		TConstVoxelArrayView<float> PositionZ);

	static FVoxelBox FromPositions(
		TConstVoxelArrayView<double> PositionX,
		TConstVoxelArrayView<double> PositionY,
		TConstVoxelArrayView<double> PositionZ);

	FORCEINLINE FVector3d Size() const
	{
		return Max - Min;
	}
	FORCEINLINE FVector3d GetCenter() const
	{
		return (Min + Max) / 2;
	}
	FORCEINLINE FVector3d GetExtent() const
	{
		return (Max - Min) / 2;
	}
	FORCEINLINE int32 GetLargestAxis() const
	{
		return FVoxelUtilities::GetLargestAxis(Size());
	}

	FString ToString() const;

	FORCEINLINE FBox ToFBox() const
	{
		return FBox(FVector(Min), FVector(Max));
	}
	FORCEINLINE FBox3f ToFBox3f() const
	{
		return FBox3f(FVector3f(Min), FVector3f(Max));
	}

	FORCEINLINE bool IsValid() const
	{
		return
			ensureVoxelSlow(FMath::IsFinite(Min.X)) &&
			ensureVoxelSlow(FMath::IsFinite(Min.Y)) &&
			ensureVoxelSlow(FMath::IsFinite(Min.Z)) &&
			ensureVoxelSlow(FMath::IsFinite(Max.X)) &&
			ensureVoxelSlow(FMath::IsFinite(Max.Y)) &&
			ensureVoxelSlow(FMath::IsFinite(Max.Z)) &&
			Min.X <= Max.X &&
			Min.Y <= Max.Y &&
			Min.Z <= Max.Z;
	}
	FORCEINLINE bool IsValidAndNotEmpty() const
	{
		return
			IsValid() &&
			*this != FVoxelBox();
	}

	FORCEINLINE bool Contains(const double X, const double Y, const double Z) const
	{
		return
			(Min.X <= X) && (X < Max.X) &&
			(Min.Y <= Y) && (Y < Max.Y) &&
			(Min.Z <= Z) && (Z < Max.Z);
	}
	FORCEINLINE bool ContainsInclusive(const double X, const double Y, const double Z) const
	{
		return
			(Min.X <= X) && (X <= Max.X) &&
			(Min.Y <= Y) && (Y <= Max.Y) &&
			(Min.Z <= Z) && (Z <= Max.Z);
	}
	FORCEINLINE bool IsInfinite() const
	{
		return Contains(FVoxelBox(FVector3d(-1e40), FVector3d(1e40)));
	}

	FORCEINLINE bool Contains(const FIntVector& Vector) const
	{
		return Contains(Vector.X, Vector.Y, Vector.Z);
	}
	FORCEINLINE bool Contains(const FVector3f& Vector) const
	{
		return Contains(Vector.X, Vector.Y, Vector.Z);
	}
	FORCEINLINE bool Contains(const FVector3d& Vector) const
	{
		return Contains(Vector.X, Vector.Y, Vector.Z);
	}

	FORCEINLINE bool ContainsInclusive(const FIntVector& Vector) const
	{
		return ContainsInclusive(Vector.X, Vector.Y, Vector.Z);
	}
	FORCEINLINE bool ContainsInclusive(const FVector3f& Vector) const
	{
		return ContainsInclusive(Vector.X, Vector.Y, Vector.Z);
	}
	FORCEINLINE bool ContainsInclusive(const FVector3d& Vector) const
	{
		return ContainsInclusive(Vector.X, Vector.Y, Vector.Z);
	}

	FORCEINLINE bool Contains(const FVoxelBox& Other) const
	{
		return
			(Min.X <= Other.Min.X) && (Other.Max.X <= Max.X) &&
			(Min.Y <= Other.Min.Y) && (Other.Max.Y <= Max.Y) &&
			(Min.Z <= Other.Min.Z) && (Other.Max.Z <= Max.Z);
	}

	FORCEINLINE bool Intersects(const FVoxelBox& Other) const
	{
		if (Min.X >= Other.Max.X || Other.Min.X >= Max.X)
		{
			return false;
		}

		if (Min.Y >= Other.Max.Y || Other.Min.Y >= Max.Y)
		{
			return false;
		}

		if (Min.Z >= Other.Max.Z || Other.Min.Z >= Max.Z)
		{
			return false;
		}

		return true;
	}

	FORCEINLINE FVoxelBox IntersectWith(const FVoxelBox& Other) const
	{
		const FVector NewMin = FVoxelUtilities::ComponentMax(Min, Other.Min);
		const FVector NewMax = FVoxelUtilities::ComponentMin(Max, Other.Max);

		if (NewMin.X >= NewMax.X ||
			NewMin.Y >= NewMax.Y ||
			NewMin.Z >= NewMax.Z)
		{
			return {};
		}

		return FVoxelBox(NewMin, NewMax);
	}
	FORCEINLINE FVoxelBox UnionWith(const FVoxelBox& Other) const
	{
		return FVoxelBox(
			FVoxelUtilities::ComponentMin(Min, Other.Min),
			FVoxelUtilities::ComponentMax(Max, Other.Max));
	}

	// union(return value, Other) = this
	TVoxelArray<FVoxelBox, TFixedAllocator<6>> Difference(const FVoxelBox& Other) const;

	FORCEINLINE double SquaredDistanceToPoint(const FVector3d& Point) const
	{
		// Accumulates the distance as we iterate axis
		double DistSquared = 0;

		// Check each axis for min/max and add the distance accordingly
		if (Point.X < Min.X)
		{
			DistSquared += FMath::Square<double>(Min.X - Point.X);
		}
		else if (Point.X > Max.X)
		{
			DistSquared += FMath::Square<double>(Point.X - Max.X);
		}

		if (Point.Y < Min.Y)
		{
			DistSquared += FMath::Square<double>(Min.Y - Point.Y);
		}
		else if (Point.Y > Max.Y)
		{
			DistSquared += FMath::Square<double>(Point.Y - Max.Y);
		}

		if (Point.Z < Min.Z)
		{
			DistSquared += FMath::Square<double>(Min.Z - Point.Z);
		}
		else if (Point.Z > Max.Z)
		{
			DistSquared += FMath::Square<double>(Point.Z - Max.Z);
		}

		return DistSquared;
	}
	FORCEINLINE double DistanceToPoint(const FVector3d& Point) const
	{
		return FMath::Sqrt(SquaredDistanceToPoint(Point));
	}

	FORCEINLINE void RayBoxIntersection(const FVector RayOrigin, const FVector RayDirection, double& OutTimeMin, double& OutTimeMax) const
	{
		const FVector Time0 = (Min - RayOrigin) / RayDirection;
		const FVector Time1 = (Max - RayOrigin) / RayDirection;

		OutTimeMin = FMath::Max3(FMath::Min(Time0.X, Time1.X), FMath::Min(Time0.Y, Time1.Y), FMath::Min(Time0.Z, Time1.Z));
		OutTimeMax = FMath::Min3(FMath::Max(Time0.X, Time1.X), FMath::Max(Time0.Y, Time1.Y), FMath::Max(Time0.Z, Time1.Z));
	}
	FORCEINLINE bool RayBoxIntersection(const FVector RayOrigin, const FVector RayDirection, double& OutTime) const
	{
		double TimeMin;
		double TimeMax;
		RayBoxIntersection(RayOrigin, RayDirection, TimeMin, TimeMax);
		OutTime = TimeMin >= 0 ? TimeMin : TimeMax;
		return TimeMax >= TimeMin;
	}
	FORCEINLINE bool RayBoxIntersection(const FVector RayOrigin, const FVector RayDirection) const
	{
		double TimeMin;
		double TimeMax;
		RayBoxIntersection(RayOrigin, RayDirection, TimeMin, TimeMax);
		return TimeMax >= TimeMin;
	}

	FORCEINLINE FVoxelBox Scale(const double S) const
	{
		const FVector A = Min * S;
		const FVector B = Max * S;

		return FVoxelBox(
			FVoxelUtilities::ComponentMin(A, B),
			FVoxelUtilities::ComponentMax(A, B));
	}
	FORCEINLINE FVoxelBox Scale(const FVector3d& S) const
	{
		const FVector A = Min * S;
		const FVector B = Max * S;

		return FVoxelBox(
			FVoxelUtilities::ComponentMin(A, B),
			FVoxelUtilities::ComponentMax(A, B));
	}

	FORCEINLINE FVoxelBox Extend(const double Amount) const
	{
		return Extend(FVector3d(Amount));
	}
	FORCEINLINE FVoxelBox Extend(const FVector3d& Amount) const
	{
		FVoxelBox Result;
		// Skip constructor checks
		Result.Min = Min - Amount;
		Result.Max = Max + Amount;

		if (Result.Min.X > Result.Max.X)
		{
			const double NewX = (Result.Min.X + Result.Max.X) / 2;
			Result.Min.X = NewX;
			Result.Max.X = NewX;
		}
		if (Result.Min.Y > Result.Max.Y)
		{
			const double NewY = (Result.Min.Y + Result.Max.Y) / 2;
			Result.Min.Y = NewY;
			Result.Max.Y = NewY;
		}
		if (Result.Min.Z > Result.Max.Z)
		{
			const double NewZ = (Result.Min.Z + Result.Max.Z) / 2;
			Result.Min.Z = NewZ;
			Result.Max.Z = NewZ;
		}
		return Result;
	}

	FORCEINLINE FVoxelBox Translate(const FVector3d& Position) const
	{
		return FVoxelBox(Min + Position, Max + Position);
	}
	FORCEINLINE FVoxelBox ShiftBy(const FVector3d& Offset) const
	{
		return Translate(Offset);
	}

	FVoxelBox TransformBy(const FMatrix& Transform) const;
	FVoxelBox TransformBy(const FTransform& Transform) const;
	FVoxelBox InverseTransformBy(const FTransform& Transform) const;

	FORCEINLINE FVoxelBox& operator*=(const double S)
	{
		*this = Scale(S);
		return *this;
	}
	FORCEINLINE FVoxelBox& operator/=(const double S)
	{
		*this = Scale(1. / S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelBox& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}

	FORCEINLINE FVoxelBox& operator+=(const FVoxelBox& Other)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Other.Min);
		Max = FVoxelUtilities::ComponentMax(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelBox& operator+=(const FVector3d& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Point);
		Max = FVoxelUtilities::ComponentMax(Max, Point);
		return *this;
	}
	FORCEINLINE FVoxelBox& operator+=(const FVector3f& Point)
	{
		return operator+=(FVector3d(Point));
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelBox& Box)
{
	return FVoxelUtilities::MurmurHash(Box);
}

FORCEINLINE FVoxelBox operator*(const FVoxelBox& Box, const double Scale)
{
	return FVoxelBox(Box) *= Scale;
}
FORCEINLINE FVoxelBox operator*(const double Scale, const FVoxelBox& Box)
{
	return FVoxelBox(Box) *= Scale;
}

FORCEINLINE FVoxelBox operator/(const FVoxelBox& Box, const double Scale)
{
	return FVoxelBox(Box) /= Scale;
}

FORCEINLINE FVoxelBox operator+(const FVoxelBox& Box, const FVoxelBox& Other)
{
	return FVoxelBox(Box) += Other;
}
FORCEINLINE FVoxelBox operator+(const FVoxelBox& Box, const FVector3f& Point)
{
	return FVoxelBox(Box) += Point;
}
FORCEINLINE FVoxelBox operator+(const FVoxelBox& Box, const FVector3d& Point)
{
	return FVoxelBox(Box) += Point;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelBox& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelOptionalBox
{
	FVoxelOptionalBox() = default;
	FORCEINLINE FVoxelOptionalBox(const FVoxelBox& Box)
		: Box(Box)
		, bValid(true)
	{
	}

	FORCEINLINE const FVoxelBox& GetBox() const
	{
		checkVoxelSlow(IsValid());
		return Box;
	}
	FORCEINLINE const FVoxelBox* operator->() const
	{
		checkVoxelSlow(IsValid());
		return &Box;
	}
	FORCEINLINE const FVoxelBox& operator*() const
	{
		checkVoxelSlow(IsValid());
		return Box;
	}

	FORCEINLINE bool IsValid() const
	{
		return bValid;
	}
	FORCEINLINE void Reset()
	{
		bValid = false;
	}

	FORCEINLINE FVoxelOptionalBox& operator=(const FVoxelBox& Other)
	{
		Box = Other;
		bValid = true;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelOptionalBox& Other) const
	{
		if (bValid != Other.bValid)
		{
			return false;
		}
		if (!bValid && !Other.bValid)
		{
			return true;
		}
		return Box == Other.Box;
	}

	FORCEINLINE FVoxelOptionalBox& operator+=(const FVoxelBox& Other)
	{
		if (bValid)
		{
			Box += Other;
		}
		else
		{
			Box = Other;
			bValid = true;
		}
		return *this;
	}
	FORCEINLINE FVoxelOptionalBox& operator+=(const FVoxelOptionalBox& Other)
	{
		if (Other.bValid)
		{
			*this += Other.GetBox();
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalBox& operator+=(const FVector3f& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelBox(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalBox& operator+=(const FVector3d& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelBox(Point);
			bValid = true;
		}
		return *this;
	}

	template<typename T>
	FORCEINLINE FVoxelOptionalBox& operator+=(const TArray<T>& Other)
	{
		for (auto& It : Other)
		{
			*this += It;
		}
		return *this;
	}

private:
	FVoxelBox Box;
	bool bValid = false;
};

template<typename T>
FORCEINLINE FVoxelOptionalBox operator+(const FVoxelOptionalBox& Box, const T& Other)
{
	FVoxelOptionalBox Copy = Box;
	return Copy += Other;
}