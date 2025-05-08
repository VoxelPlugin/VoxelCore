// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelBox2D.generated.h"

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelBox2D
{
	GENERATED_BODY()

	// Included
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	FVector2D Min = FVector2D(ForceInit);

	// Included (has to be otherwise FVoxelBox2D(X).Contains(X) is false)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
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

	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FIntPoint> Positions);
	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FVector2f> Positions);
	static FVoxelBox2D FromPositions(TConstVoxelArrayView<FVector2d> Positions);

	static FVoxelBox2D FromPositions(
		TConstVoxelArrayView<float> PositionX,
		TConstVoxelArrayView<float> PositionY);

	static FVoxelBox2D FromPositions(
		TConstVoxelArrayView<double> PositionX,
		TConstVoxelArrayView<double> PositionY);

	FORCEINLINE FVector2D Size() const
	{
		return Max - Min;
	}
	FORCEINLINE FVector2D GetCenter() const
	{
		return (Min + Max) / 2;
	}

	FString ToString() const;

	FORCEINLINE FBox2D ToFBox() const
	{
		return FBox2D(FVector2D(Min), FVector2D(Max));
	}
	FORCEINLINE FBox2f ToFBox2f() const
	{
		return FBox2f(FVector2f(Min), FVector2f(Max));
	}
	FORCEINLINE FVoxelBox ToBox3D(const double MinZ, const double MaxZ) const
	{
		return FVoxelBox(
			FVector3d(Min.X, Min.Y, MinZ),
			FVector3d(Max.X, Max.Y, MaxZ));
	}
	FORCEINLINE FVoxelBox ToBox3D(const FVoxelInterval& BoundsZ) const
	{
		return ToBox3D(BoundsZ.Min, BoundsZ.Max);
	}
	FORCEINLINE FVoxelBox ToBox3D_Infinite() const
	{
		return ToBox3D(FVoxelBox::Infinite.GetZ());
	}

	FORCEINLINE FVoxelInterval GetX() const
	{
		return { Min.X, Max.X };
	}
	FORCEINLINE FVoxelInterval GetY() const
	{
		return { Min.Y, Max.Y };
	}

	FORCEINLINE bool IsValid() const
	{
		return
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Min.X)) &&
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Min.Y)) &&
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Max.X)) &&
			ensureVoxelSlow(FVoxelUtilities::IsFinite(Max.Y)) &&
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

	FORCEINLINE bool Intersects(const FVoxelBox2D& Other) const
	{
		if (Min.X > Other.Max.X || Other.Min.X > Max.X)
		{
			return false;
		}

		if (Min.Y > Other.Max.Y || Other.Min.Y > Max.Y)
		{
			return false;
		}

		return true;
	}
	FORCEINLINE bool IntersectsSphere(const FVector2D& Center, const double Radius) const
	{
		return SquaredDistanceToPoint(Center) <= FMath::Square(Radius);
	}
	FORCEINLINE bool IsInsideSphere(const FVector2D& Center, const double Radius) const
	{
		const double RadiusSquared = FMath::Square(Radius);

		const FVector2D MinSquared = FMath::Square(Min - Center);
		const FVector2D MaxSquared = FMath::Square(Max - Center);

		return
			MinSquared.X + MinSquared.Y <= RadiusSquared &&
			MaxSquared.X + MinSquared.Y <= RadiusSquared &&
			MinSquared.X + MaxSquared.Y <= RadiusSquared &&
			MaxSquared.X + MaxSquared.Y <= RadiusSquared;
	}

	FORCEINLINE FVoxelBox2D IntersectWith(const FVoxelBox2D& Other) const
	{
		const FVector2D NewMin = FVoxelUtilities::ComponentMax(Min, Other.Min);
		const FVector2D NewMax = FVoxelUtilities::ComponentMin(Max, Other.Max);

		if (NewMin.X > NewMax.X ||
			NewMin.Y > NewMax.Y)
		{
			return {};
		}

		return FVoxelBox2D(NewMin, NewMax);
	}
	FORCEINLINE FVoxelBox2D UnionWith(const FVoxelBox2D& Other) const
	{
		return FVoxelBox2D(
			FVoxelUtilities::ComponentMin(Min, Other.Min),
			FVoxelUtilities::ComponentMax(Max, Other.Max));
	}

	FVoxelBox2D Remove_Union(const FVoxelBox2D& Other) const;

	void Remove_Split(
		const FVoxelBox2D& Other,
		TVoxelArray<FVoxelBox2D>& OutRemainder) const;

	FORCEINLINE double SquaredDistanceToPoint(const FVector2D& Point) const
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
	FORCEINLINE double DistanceToPoint(const FVector2D& Point) const
	{
		return FMath::Sqrt(SquaredDistanceToPoint(Point));
	}

	FORCEINLINE FVoxelBox2D Scale(const double S) const
	{
		const FVector2D A = Min * S;
		const FVector2D B = Max * S;

		return FVoxelBox2D(
			FVoxelUtilities::ComponentMin(A, B),
			FVoxelUtilities::ComponentMax(A, B));
	}
	FORCEINLINE FVoxelBox2D Scale(const FVector2D& S) const
	{
		const FVector2D A = Min * S;
		const FVector2D B = Max * S;

		return FVoxelBox2D(
			FVoxelUtilities::ComponentMin(A, B),
			FVoxelUtilities::ComponentMax(A, B));
	}
	FORCEINLINE FVoxelBox2D Extend(const double Amount) const
	{
		return { Min - Amount, Max + Amount };
	}
	FORCEINLINE FVoxelBox2D Extend(const FVector2D& Amount) const
	{
		FVoxelBox2D Result;
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
		return Result;
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

	FORCEINLINE FVoxelBox2D& operator*=(const double S)
	{
		*this = Scale(S);
		return *this;
	}
	FORCEINLINE FVoxelBox2D& operator/=(const double S)
	{
		*this = Scale(1. / S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelBox2D& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
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