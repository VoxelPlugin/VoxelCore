// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"
#include "VoxelBox.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelBox
{
	GENERATED_BODY()

	UPROPERTY()
	FVector3d Min = FVector3d(ForceInit);

	UPROPERTY()
	FVector3d Max = FVector3d(ForceInit);

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

	template<typename T>
	explicit FVoxelBox(const TArray<T>& Data)
	{
		if (!ensure(Data.Num() > 0))
		{
			Min = Max = FVector3d::ZeroVector;
			return;
		}

		*this = FVoxelBox(Data[0]);
		for (int32 Index = 1; Index < Data.Num(); Index++)
		{
			*this = *this + Data[Index];
		}
	}

	static FVoxelBox FromPositions(TConstVoxelArrayView<FIntVector> Positions);
	static FVoxelBox FromPositions(TConstVoxelArrayView<FVector3f> Positions);
	static FVoxelBox FromPositions(TConstVoxelArrayView<FVector3d> Positions);

	static FVoxelBox FromPositions(
		TConstVoxelArrayView<float> PositionX,
		TConstVoxelArrayView<float> PositionY,
		TConstVoxelArrayView<float> PositionZ);

	FORCEINLINE static FVoxelBox SafeConstruct(const FVector3d& A, const FVector3d& B)
	{
		FVoxelBox Box;
		Box.Min = FVoxelUtilities::ComponentMin(A, B);
		Box.Max = FVoxelUtilities::ComponentMax(A, B);
		return Box;
	}

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

	FString ToString() const
	{
		return FString::Printf(TEXT("(%f/%f, %f/%f, %f/%f)"), Min.X, Max.X, Min.Y, Max.Y, Min.Z, Max.Z);
	}
	FBox ToFBox() const
	{
		return FBox(FVector(Min), FVector(Max));
	}
	FBox3f ToFBox3f() const
	{
		return FBox3f(FVector3f(Min), FVector3f(Max));
	}

	FORCEINLINE bool IsValid() const
	{
		return
			ensure(FMath::IsFinite(Min.X)) &&
			ensure(FMath::IsFinite(Min.Y)) &&
			ensure(FMath::IsFinite(Min.Z)) &&
			ensure(FMath::IsFinite(Max.X)) &&
			ensure(FMath::IsFinite(Max.Y)) &&
			ensure(FMath::IsFinite(Max.Z)) &&
			Min.X <= Max.X &&
			Min.Y <= Max.Y &&
			Min.Z <= Max.Z;
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

	FORCEINLINE bool Intersect(const FVoxelBox& Other) const
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

	// Return the intersection of the two boxes
	FVoxelBox Overlap(const FVoxelBox& Other) const
	{
		if (!Intersect(Other))
		{
			return {};
		}

		FVector3d NewMin;
		FVector3d NewMax;

		NewMin.X = FMath::Max(Min.X, Other.Min.X);
		NewMax.X = FMath::Min(Max.X, Other.Max.X);

		NewMin.Y = FMath::Max(Min.Y, Other.Min.Y);
		NewMax.Y = FMath::Min(Max.Y, Other.Max.Y);

		NewMin.Z = FMath::Max(Min.Z, Other.Min.Z);
		NewMax.Z = FMath::Min(Max.Z, Other.Max.Z);

		return FVoxelBox(NewMin, NewMax);
	}
	FVoxelBox Union(const FVoxelBox& Other) const
	{
		FVector3d NewMin;
		FVector3d NewMax;

		NewMin.X = FMath::Min(Min.X, Other.Min.X);
		NewMax.X = FMath::Max(Max.X, Other.Max.X);

		NewMin.Y = FMath::Min(Min.Y, Other.Min.Y);
		NewMax.Y = FMath::Max(Max.Y, Other.Max.Y);

		NewMin.Z = FMath::Min(Min.Z, Other.Min.Z);
		NewMax.Z = FMath::Max(Max.Z, Other.Max.Z);

		return FVoxelBox(NewMin, NewMax);
	}

	FORCEINLINE double ComputeSquaredDistanceFromBoxToPoint(const FVector3d& Point) const
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
	FORCEINLINE double DistanceFromBoxToPoint(const FVector3d& Point) const
	{
		return FMath::Sqrt(ComputeSquaredDistanceFromBoxToPoint(Point));
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
		return SafeConstruct(Min * S, Max * S);
	}
	FORCEINLINE FVoxelBox Scale(const FVector3d& S) const
	{
		return SafeConstruct(Min * S, Max * S);
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

	FORCEINLINE FVoxelBox& operator*=(const double Scale)
	{
		Min *= Scale;
		Max *= Scale;
		return *this;
	}
	FORCEINLINE FVoxelBox& operator/=(const double Scale)
	{
		Min /= Scale;
		Max /= Scale;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelBox& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}
	FORCEINLINE bool operator!=(const FVoxelBox& Other) const
	{
		return Min != Other.Min || Max != Other.Max;
	}

	template<typename MatrixType>
	FVoxelBox TransformByImpl(const MatrixType& Transform) const
	{
		if (IsInfinite())
		{
			return Infinite;
		}

		FVector3d Vertices[8] =
		{
			FVector3d(Min.X, Min.Y, Min.Z),
			FVector3d(Max.X, Min.Y, Min.Z),
			FVector3d(Min.X, Max.Y, Min.Z),
			FVector3d(Max.X, Max.Y, Min.Z),
			FVector3d(Min.X, Min.Y, Max.Z),
			FVector3d(Max.X, Min.Y, Max.Z),
			FVector3d(Min.X, Max.Y, Max.Z),
			FVector3d(Max.X, Max.Y, Max.Z)
		};

		for (int32 Index = 0; Index < 8; Index++)
		{
			Vertices[Index] = FVector3d(Transform.TransformPosition(FVector(Vertices[Index])));
		}

		FVoxelBox NewBox;
		NewBox.Min = Vertices[0];
		NewBox.Max = Vertices[0];

		for (int32 Index = 1; Index < 8; Index++)
		{
			NewBox.Min = FVoxelUtilities::ComponentMin(NewBox.Min, Vertices[Index]);
			NewBox.Max = FVoxelUtilities::ComponentMax(NewBox.Max, Vertices[Index]);
		}

		return NewBox;
	}

	FVoxelBox TransformBy(const FMatrix44f& Transform) const
	{
		return TransformByImpl(FMatrix44d(Transform));
	}
	FVoxelBox TransformBy(const FTransform3f& Transform) const
	{
		return TransformByImpl(FTransform3d(Transform));
	}
	FVoxelBox TransformBy(const FMatrix44d& Transform) const
	{
		return TransformByImpl(Transform);
	}
	FVoxelBox TransformBy(const FTransform3d& Transform) const
	{
		return TransformByImpl(Transform);
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
	FORCEINLINE bool operator!=(const FVoxelOptionalBox& Other) const
	{
		return !(*this == Other);
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