// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "Math/TransformCalculus2D.h"
#include "VoxelBox2D.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelBox2D
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Min = FVector2D(ForceInit);

	UPROPERTY()
	FVector2D Max = FVector2D(ForceInit);

	static const FVoxelBox2D Infinite;
	static const FVoxelBox2D InvertedInfinite;

	FVoxelBox2D() = default;

	FORCEINLINE FVoxelBox2D(const FVector2D& Min, const FVector2D& Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min.X <= Max.X);
		ensureVoxelSlow(Min.Y <= Max.Y);
	}
	FORCEINLINE explicit FVoxelBox2D(const double Min, const FVector2D& Max)
		: FVoxelBox2D(FVector2D(Min), Max)
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const FVector2D& Min, const double Max)
		: FVoxelBox2D(Min, FVector2D(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const FVector2f& Min, const FVector2f& Max)
		: FVoxelBox2D(FVector2D(Min), FVector2D(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const FIntPoint& Min, const FIntPoint& Max)
		: FVoxelBox2D(FVector2D(Min), FVector2D(Max))
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const double Min, const double Max)
		: FVoxelBox2D(FVector2D(Min), FVector2D(Max))
	{
	}

	FORCEINLINE explicit FVoxelBox2D(const FVector2f& Position)
		: FVoxelBox2D(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const FVector2D& Position)
		: FVoxelBox2D(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelBox2D(const FIntPoint& Position)
		: FVoxelBox2D(Position, Position)
	{
	}

	FORCEINLINE explicit FVoxelBox2D(const FBox2f& Box)
		: FVoxelBox2D(Box.Min, Box.Max)
	{
		ensureVoxelSlow(Box.bIsValid);
	}
	FORCEINLINE explicit FVoxelBox2D(const FBox2d& Box)
		: FVoxelBox2D(Box.Min, Box.Max)
	{
		ensureVoxelSlow(Box.bIsValid);
	}
	FORCEINLINE explicit FVoxelBox2D(const FVoxelBox& Box)
		: FVoxelBox2D(FVector2D(Box.Min), FVector2D(Box.Max))
	{
	}

	template<typename T>
	explicit FVoxelBox2D(const TArray<T>& Data)
	{
		if (!ensure(Data.Num() > 0))
		{
			Min = Max = FVector2D::ZeroVector;
			return;
		}

		*this = FVoxelBox2D(Data[0]);
		for (int32 Index = 1; Index < Data.Num(); Index++)
		{
			*this = *this + Data[Index];
		}
	}

	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FIntPoint> Positions);
	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FVector2f> Positions);
	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FVector2d> Positions);

	static FVoxelBox2D FromPositions(
		TConstVoxelArrayView<float> PositionX,
		TConstVoxelArrayView<float> PositionY);

	FORCEINLINE FVector2D Size() const
	{
		return Max - Min;
	}
	FORCEINLINE FVector2D GetCenter() const
	{
		return (Min + Max) / 2;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%f/%f, %f/%f)"), Min.X, Max.X, Min.Y, Max.Y);
	}
	FBox2D ToFBox() const
	{
		return FBox2D(FVector2D(Min), FVector2D(Max));
	}
	FBox2f ToFBox2f() const
	{
		return FBox2f(FVector2f(Min), FVector2f(Max));
	}
	FVoxelBox ToBox3D(const double MinZ, const double MaxZ) const
	{
		return FVoxelBox(
			FVector3d(Min.X, Min.Y, MinZ),
			FVector3d(Max.X, Max.Y, MaxZ));
	}
	FVoxelBox ToBox3D_Infinite() const
	{
		return ToBox3D(FVoxelBox::Infinite.Min.Z, FVoxelBox::Infinite.Max.Z);
	}

	FORCEINLINE bool IsValid() const
	{
		return
			ensure(FMath::IsFinite(Min.X)) &&
			ensure(FMath::IsFinite(Min.Y)) &&
			ensure(FMath::IsFinite(Max.X)) &&
			ensure(FMath::IsFinite(Max.Y)) &&
			Min.X <= Max.X &&
			Min.Y <= Max.Y;
	}
	FORCEINLINE bool IsValidAndNotEmpty() const
	{
		return
			IsValid() &&
			*this != FVoxelBox2D();
	}

	FORCEINLINE bool Contains(const double X, const double Y) const
	{
		return
			(Min.X <= X) && (X <= Max.X) &&
			(Min.Y <= Y) && (Y <= Max.Y);
	}
	FORCEINLINE bool IsInfinite() const
	{
		return Contains(Infinite.Extend(-1000));
	}

	FORCEINLINE bool Contains(const FIntPoint& Vector) const
	{
		return Contains(Vector.X, Vector.Y);
	}
	FORCEINLINE bool Contains(const FVector2f& Vector) const
	{
		return Contains(Vector.X, Vector.Y);
	}
	FORCEINLINE bool Contains(const FVector2D& Vector) const
	{
		return Contains(Vector.X, Vector.Y);
	}

	FORCEINLINE bool Contains(const FVoxelBox2D& Other) const
	{
		return
			(Min.X <= Other.Min.X) && (Other.Max.X <= Max.X) &&
			(Min.Y <= Other.Min.Y) && (Other.Max.Y <= Max.Y);
	}

	FORCEINLINE bool Intersect(const FVoxelBox2D& Other) const
	{
		if (Min.X >= Other.Max.X || Other.Min.X >= Max.X)
		{
			return false;
		}

		if (Min.Y >= Other.Max.Y || Other.Min.Y >= Max.Y)
		{
			return false;
		}

		return true;
	}

	// Return the intersection of the two boxes
	FVoxelBox2D Overlap(const FVoxelBox2D& Other) const
	{
		if (!Intersect(Other))
		{
			return {};
		}

		FVector2D NewMin;
		FVector2D NewMax;

		NewMin.X = FMath::Max(Min.X, Other.Min.X);
		NewMax.X = FMath::Min(Max.X, Other.Max.X);

		NewMin.Y = FMath::Max(Min.Y, Other.Min.Y);
		NewMax.Y = FMath::Min(Max.Y, Other.Max.Y);

		return FVoxelBox2D(NewMin, NewMax);
	}
	FVoxelBox2D Union(const FVoxelBox2D& Other) const
	{
		FVector2D NewMin;
		FVector2D NewMax;

		NewMin.X = FMath::Min(Min.X, Other.Min.X);
		NewMax.X = FMath::Max(Max.X, Other.Max.X);

		NewMin.Y = FMath::Min(Min.Y, Other.Min.Y);
		NewMax.Y = FMath::Max(Max.Y, Other.Max.Y);

		return FVoxelBox2D(NewMin, NewMax);
	}

	FORCEINLINE double ComputeSquaredDistanceFromBoxToPoint(const FVector2D& Point) const
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

		return DistSquared;
	}
	FORCEINLINE double DistanceFromBoxToPoint(const FVector2D& Point) const
	{
		return FMath::Sqrt(ComputeSquaredDistanceFromBoxToPoint(Point));
	}

	FORCEINLINE FVoxelBox2D Scale(const double S) const
	{
		return { Min * S, Max * S };
	}
	FORCEINLINE FVoxelBox2D Scale(const FVector2D& S) const
	{
		return { Min * S, Max * S };
	}
	FORCEINLINE FVoxelBox2D Extend(const double Amount) const
	{
		return { Min - Amount, Max + Amount };
	}
	FORCEINLINE FVoxelBox2D Translate(const FVector2D& Position) const
	{
		return FVoxelBox2D(Min + Position, Max + Position);
	}
	FORCEINLINE FVoxelBox2D ShiftBy(const FVector2D& Offset) const
	{
		return Translate(Offset);
	}

	FVoxelBox2D TransformBy(const FTransform2d& Transform) const;

	FORCEINLINE FVoxelBox2D& operator*=(const double Scale)
	{
		Min *= Scale;
		Max *= Scale;
		return *this;
	}
	FORCEINLINE FVoxelBox2D& operator/=(const double Scale)
	{
		Min /= Scale;
		Max /= Scale;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelBox2D& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}
	FORCEINLINE bool operator!=(const FVoxelBox2D& Other) const
	{
		return Min != Other.Min || Max != Other.Max;
	}

	FORCEINLINE FVoxelBox2D& operator+=(const FVoxelBox2D& Other)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Other.Min);
		Max = FVoxelUtilities::ComponentMax(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelBox2D& operator+=(const FVector2D& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Point);
		Max = FVoxelUtilities::ComponentMax(Max, Point);
		return *this;
	}
	FORCEINLINE FVoxelBox2D& operator+=(const FVector2f& Point)
	{
		return operator+=(FVector2D(Point));
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelBox2D& Box)
{
	return FVoxelUtilities::MurmurHash(Box);
}

FORCEINLINE FVoxelBox2D operator*(const FVoxelBox2D& Box, const double Scale)
{
	return FVoxelBox2D(Box) *= Scale;
}
FORCEINLINE FVoxelBox2D operator*(const double Scale, const FVoxelBox2D& Box)
{
	return FVoxelBox2D(Box) *= Scale;
}

FORCEINLINE FVoxelBox2D operator/(const FVoxelBox2D& Box, const double Scale)
{
	return FVoxelBox2D(Box) /= Scale;
}

FORCEINLINE FVoxelBox2D operator+(const FVoxelBox2D& Box, const FVoxelBox2D& Other)
{
	return FVoxelBox2D(Box) += Other;
}
FORCEINLINE FVoxelBox2D operator+(const FVoxelBox2D& Box, const FVector2f& Point)
{
	return FVoxelBox2D(Box) += Point;
}
FORCEINLINE FVoxelBox2D operator+(const FVoxelBox2D& Box, const FVector2D& Point)
{
	return FVoxelBox2D(Box) += Point;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelBox2D& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelOptionalBox2D
{
	FVoxelOptionalBox2D() = default;
	FVoxelOptionalBox2D(const FVoxelBox2D& Box)
		: Box(Box)
		, bValid(true)
	{
	}

	FORCEINLINE const FVoxelBox2D& GetBox() const
	{
		check(IsValid());
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

	FORCEINLINE FVoxelOptionalBox2D& operator=(const FVoxelBox2D& Other)
	{
		Box = Other;
		bValid = true;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelOptionalBox2D& Other) const
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
	FORCEINLINE bool operator!=(const FVoxelOptionalBox2D& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE FVoxelOptionalBox2D& operator+=(const FVoxelBox2D& Other)
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
	FORCEINLINE FVoxelOptionalBox2D& operator+=(const FVoxelOptionalBox2D& Other)
	{
		if (Other.bValid)
		{
			*this += Other.GetBox();
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalBox2D& operator+=(const FVector2f& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelBox2D(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalBox2D& operator+=(const FVector2D& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelBox2D(Point);
			bValid = true;
		}
		return *this;
	}

	template<typename T>
	FORCEINLINE FVoxelOptionalBox2D& operator+=(const TArray<T>& Other)
	{
		for (auto& It : Other)
		{
			*this += It;
		}
		return *this;
	}

private:
	FVoxelBox2D Box;
	bool bValid = false;
};

template<typename T>
FORCEINLINE FVoxelOptionalBox2D operator+(const FVoxelOptionalBox2D& Box, const T& Other)
{
	FVoxelOptionalBox2D Copy = Box;
	return Copy += Other;
}