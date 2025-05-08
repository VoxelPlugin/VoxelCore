// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"
#include "VoxelMinimal/Utilities/VoxelIntVectorUtilities.h"
#include "VoxelIntBox.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelIntBox
{
	GENERATED_BODY()

	// Inclusive
	UPROPERTY()
	FIntVector Min = FIntVector(ForceInit);

	// Exclusive
	UPROPERTY()
	FIntVector Max = FIntVector(ForceInit);

	static const FVoxelIntBox Infinite;
	static const FVoxelIntBox InvertedInfinite;

	FVoxelIntBox() = default;

	FORCEINLINE FVoxelIntBox(const FIntVector& Min, const FIntVector& Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min.X <= Max.X);
		ensureVoxelSlow(Min.Y <= Max.Y);
		ensureVoxelSlow(Min.Z <= Max.Z);
	}
	FORCEINLINE explicit FVoxelIntBox(const int32 Min, const FIntVector& Max)
		: FVoxelIntBox(FIntVector(Min), Max)
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const FIntVector& Min, const int32 Max)
		: FVoxelIntBox(Min, FIntVector(Max))
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const int32 Min, const int32 Max)
		: FVoxelIntBox(FIntVector(Min), FIntVector(Max))
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const FVector3f& Min, const FVector3f& Max)
		: FVoxelIntBox(FVoxelUtilities::FloorToInt(Min), FVoxelUtilities::CeilToInt(Max) + 1)
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const FVector3d& Min, const FVector3d& Max)
		: FVoxelIntBox(FVoxelUtilities::FloorToInt(Min), FVoxelUtilities::CeilToInt(Max) + 1)
	{
	}

	FORCEINLINE explicit FVoxelIntBox(const FVector3f& Position)
		: FVoxelIntBox(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const FVector3d& Position)
		: FVoxelIntBox(Position, Position)
	{
	}

	FORCEINLINE explicit FVoxelIntBox(const FIntVector& Position)
		: FVoxelIntBox(Position, Position + 1)
	{
	}

	FORCEINLINE explicit FVoxelIntBox(const int32 X, const int32 Y, const int32 Z)
		: FVoxelIntBox(FIntVector(X, Y, Z), FIntVector(X + 1, Y + 1, Z + 1))
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const float X, const float Y, const float Z)
		: FVoxelIntBox(FVector3f(X, Y, Z), FVector3f(X + 1, Y + 1, Z + 1))
	{
	}
	FORCEINLINE explicit FVoxelIntBox(const double X, const double Y, const double Z)
		: FVoxelIntBox(FVector3d(X, Y, Z), FVector3d(X + 1, Y + 1, Z + 1))
	{
	}

	static FVoxelIntBox FromPositions(TConstVoxelArrayView<FIntVector> Positions);

	static FVoxelIntBox FromPositions(
		TConstVoxelArrayView<int32> PositionX,
		TConstVoxelArrayView<int32> PositionY,
		TConstVoxelArrayView<int32> PositionZ);

	template<typename T>
	FORCEINLINE static FVoxelIntBox FromFloatBox_NoPadding(T Box)
	{
		return
		{
			FVoxelUtilities::FloorToInt(Box.Min),
			FVoxelUtilities::CeilToInt(Box.Max),
		};
	}
	template<typename T>
	FORCEINLINE static FVoxelIntBox FromFloatBox_WithPadding(T Box)
	{
		return
		{
			FVoxelUtilities::FloorToInt(Box.Min),
			FVoxelUtilities::CeilToInt(Box.Max) + 1,
		};
	}

	FORCEINLINE FIntVector Size() const
	{
		checkVoxelSlow(SizeIs32Bit());
		return Max - Min;
	}
	FORCEINLINE FVector GetCenter() const
	{
		return FVector(Min + Max) / 2.f;
	}
	FORCEINLINE double Count_double() const
	{
		return
			(double(Max.X) - double(Min.X)) *
			(double(Max.Y) - double(Min.Y)) *
			(double(Max.Z) - double(Min.Z));
	}
	FORCEINLINE uint64 Count_uint64() const
	{
		checkVoxelSlow(int64(Max.X) - int64(Min.X) < (1llu << 21));
		checkVoxelSlow(int64(Max.Y) - int64(Min.Y) < (1llu << 21));
		checkVoxelSlow(int64(Max.Z) - int64(Min.Z) < (1llu << 21));

		return
			uint64(Max.X - Min.X) *
			uint64(Max.Y - Min.Y) *
			uint64(Max.Z - Min.Z);
	}
	FORCEINLINE int32 Count_int32() const
	{
		checkVoxelSlow(Count_uint64() <= MAX_int32);

		return
			int32(Max.X - Min.X) *
			int32(Max.Y - Min.Y) *
			int32(Max.Z - Min.Z);
	}

	FORCEINLINE bool SizeIs32Bit() const
	{
		return
			int64(Max.X) - int64(Min.X) < MAX_int32 &&
			int64(Max.Y) - int64(Min.Y) < MAX_int32 &&
			int64(Max.Z) - int64(Min.Z) < MAX_int32;
	}

	FORCEINLINE bool IsInfinite() const
	{
		// Not exactly accurate, but should be safe
		const int32 InfiniteMin = MIN_int32 / 2;
		const int32 InfiniteMax = MAX_int32 / 2;
		return
			Min.X < InfiniteMin ||
			Min.Y < InfiniteMin ||
			Min.Z < InfiniteMin ||
			Max.X > InfiniteMax ||
			Max.Y > InfiniteMax ||
			Max.Z > InfiniteMax;
	}

	FString ToString() const;

	FORCEINLINE FVoxelBox ToVoxelBox() const
	{
		return FVoxelBox(Min, Max);
	}
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
		return Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z;
	}

	FORCEINLINE bool Contains(const int32 X, const int32 Y, const int32 Z) const
	{
		return
			(X >= Min.X) && (X < Max.X) &&
			(Y >= Min.Y) && (Y < Max.Y) &&
			(Z >= Min.Z) && (Z < Max.Z);
	}
	FORCEINLINE bool Contains(const FIntVector& Point) const
	{
		return
			(Point.X >= Min.X) && (Point.X < Max.X) &&
			(Point.Y >= Min.Y) && (Point.Y < Max.Y) &&
			(Point.Z >= Min.Z) && (Point.Z < Max.Z);
	}
	FORCEINLINE bool Contains(const FVoxelIntBox& Other) const
	{
		return
			Min.X <= Other.Min.X &&
			Min.Y <= Other.Min.Y &&
			Min.Z <= Other.Min.Z &&
			Max.X >= Other.Max.X &&
			Max.Y >= Other.Max.Y &&
			Max.Z >= Other.Max.Z;
	}

	template<typename T>
	bool Contains(T X, T Y, T Z) const = delete;

	FORCEINLINE FIntVector Clamp(FIntVector Point, const int32 Step = 1) const
	{
		Clamp(Point.X, Point.Y, Point.Z, Step);
		return Point;
	}
	FORCEINLINE void Clamp(int32& X, int32& Y, int32& Z, const int32 Step = 1) const
	{
		X = FMath::Clamp(X, Min.X, Max.X - Step);
		Y = FMath::Clamp(Y, Min.Y, Max.Y - Step);
		Z = FMath::Clamp(Z, Min.Z, Max.Z - Step);
		ensureVoxelSlowNoSideEffects(Contains(X, Y, Z));
	}

	FORCEINLINE FVoxelIntBox Clamp(const FVoxelIntBox& Other) const
	{
		// It's not valid to call Clamp if we're not intersecting Other
		ensureVoxelSlowNoSideEffects(Intersects(Other));

		FVoxelIntBox Result;

		Result.Min.X = FMath::Clamp(Other.Min.X, Min.X, Max.X - 1);
		Result.Min.Y = FMath::Clamp(Other.Min.Y, Min.Y, Max.Y - 1);
		Result.Min.Z = FMath::Clamp(Other.Min.Z, Min.Z, Max.Z - 1);

		Result.Max.X = FMath::Clamp(Other.Max.X, Min.X + 1, Max.X);
		Result.Max.Y = FMath::Clamp(Other.Max.Y, Min.Y + 1, Max.Y);
		Result.Max.Z = FMath::Clamp(Other.Max.Z, Min.Z + 1, Max.Z);

		ensureVoxelSlowNoSideEffects(Other.Contains(Result));
		return Result;
	}

	FORCEINLINE bool Intersects(const FVoxelIntBox& Other) const
	{
		if ((Min.X >= Other.Max.X) || (Other.Min.X >= Max.X))
		{
			return false;
		}

		if ((Min.Y >= Other.Max.Y) || (Other.Min.Y >= Max.Y))
		{
			return false;
		}

		if ((Min.Z >= Other.Max.Z) || (Other.Min.Z >= Max.Z))
		{
			return false;
		}

		return true;
	}

	FORCEINLINE FVoxelIntBox IntersectWith(const FVoxelIntBox& Other) const
	{
		const FIntVector NewMin = FVoxelUtilities::ComponentMax(Min, Other.Min);
		const FIntVector NewMax = FVoxelUtilities::ComponentMin(Max, Other.Max);

		if (NewMin.X >= NewMax.X ||
			NewMin.Y >= NewMax.Y ||
			NewMin.Z >= NewMax.Z)
		{
			return {};
		}

		return FVoxelIntBox(NewMin, NewMax);
	}
	FORCEINLINE FVoxelIntBox UnionWith(const FVoxelIntBox& Other) const
	{
		return FVoxelIntBox(
			FVoxelUtilities::ComponentMin(Min, Other.Min),
			FVoxelUtilities::ComponentMax(Max, Other.Max));
	}

	FVoxelIntBox Remove_Union(const FVoxelIntBox& Other) const;

	void Remove_Split(
		const FVoxelIntBox& Other,
		TVoxelArray<FVoxelIntBox>& OutRemainder) const;

	FORCEINLINE double SquaredDistanceToPoint(const FVector& Point) const
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
	FORCEINLINE uint64 SquaredDistanceToPoint(const FIntVector& Point) const
	{
		// Accumulates the distance as we iterate axis
		uint64 DistSquared = 0;

		// Check each axis for min/max and add the distance accordingly
		if (Point.X < Min.X)
		{
			DistSquared += FMath::Square<uint64>(Min.X - Point.X);
		}
		else if (Point.X > Max.X)
		{
			DistSquared += FMath::Square<uint64>(Point.X - Max.X);
		}

		if (Point.Y < Min.Y)
		{
			DistSquared += FMath::Square<uint64>(Min.Y - Point.Y);
		}
		else if (Point.Y > Max.Y)
		{
			DistSquared += FMath::Square<uint64>(Point.Y - Max.Y);
		}

		if (Point.Z < Min.Z)
		{
			DistSquared += FMath::Square<uint64>(Min.Z - Point.Z);
		}
		else if (Point.Z > Max.Z)
		{
			DistSquared += FMath::Square<uint64>(Point.Z - Max.Z);
		}

		return DistSquared;
	}
	FORCEINLINE double DistanceToPoint(const FVector& Point) const
	{
		return FMath::Sqrt(SquaredDistanceToPoint(Point));
	}

	FORCEINLINE bool IsMultipleOf(const int32 Step) const
	{
		return
			Min % Step == 0 &&
			Max % Step == 0;
	}

	// OldBox included in NewBox, but NewBox not included in OldBox
	FORCEINLINE FVoxelIntBox MakeMultipleOfBigger(const int32 Step) const
	{
		FVoxelIntBox NewBox;
		NewBox.Min = FVoxelUtilities::DivideFloor(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step) * Step;
		return NewBox;
	}
	// NewBox included in OldBox, but OldBox not included in NewBox
	FORCEINLINE FVoxelIntBox MakeMultipleOfSmaller(const int32 Step) const
	{
		FVoxelIntBox NewBox;
		NewBox.Min = FVoxelUtilities::DivideCeil(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideFloor(Max, Step) * Step;
		return NewBox;
	}
	FORCEINLINE FVoxelIntBox MakeMultipleOfRoundUp(const int32 Step) const
	{
		FVoxelIntBox NewBox;
		NewBox.Min = FVoxelUtilities::DivideCeil(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step) * Step;
		return NewBox;
	}

	FORCEINLINE FVoxelIntBox DivideBigger(const int32 Step) const
	{
		FVoxelIntBox NewBox;
		NewBox.Min = FVoxelUtilities::DivideFloor(Min, Step);
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step);
		return NewBox;
	}
	FORCEINLINE FVoxelIntBox DivideExact(const int32 Step) const
	{
		checkVoxelSlow(Min % Step == 0);
		checkVoxelSlow(Max % Step == 0);

		FVoxelIntBox NewBox;
		NewBox.Min = Min / Step;
		NewBox.Max = Max / Step;
		return NewBox;
	}

	// Guarantee: union(OutChilds).Contains(this)
	bool Subdivide(
		int32 ChildrenSize,
		TVoxelArray<FVoxelIntBox>& OutChildren,
		bool bUseOverlap,
		int32 MaxChildren = -1) const;

	FORCEINLINE FVoxelIntBox Scale(float S) const = delete;
	FORCEINLINE FVoxelIntBox Scale(const int32 S) const
	{
		ensureVoxelSlow(S >= 0);
		return { Min * S, Max * S };
	}
	FORCEINLINE FVoxelIntBox Scale(const FVector& S) const = delete;

	FORCEINLINE FVoxelIntBox Extend(const FIntVector& Amount) const
	{
		return { Min - Amount, Max + Amount };
	}
	FORCEINLINE FVoxelIntBox Extend(const int32 Amount) const
	{
		return Extend(FIntVector(Amount));
	}
	FORCEINLINE FVoxelIntBox Translate(const FIntVector& Position) const
	{
		return FVoxelIntBox(Min + Position, Max + Position);
	}
	FORCEINLINE FVoxelIntBox ShiftBy(const FIntVector& Offset) const
	{
		return Translate(Offset);
	}

	FORCEINLINE FVoxelIntBox RemoveTranslation() const
	{
		return FVoxelIntBox(0, Max - Min);
	}

	FORCEINLINE FVoxelIntBox& operator*=(const int32 S)
	{
		*this = Scale(S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelIntBox& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, bool>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FIntVector&)>
	)
	FORCEINLINE void Iterate(LambdaType&& Lambda) const
	{
		for (int32 X = Min.X; X < Max.X; X++)
		{
			for (int32 Y = Min.Y; Y < Max.Y; Y++)
			{
				for (int32 Z = Min.Z; Z < Max.Z; Z++)
				{
					if constexpr (std::is_void_v<ReturnType>)
					{
						Lambda(FIntVector(X, Y, Z));
					}
					else
					{
						if (!Lambda(FIntVector(X, Y, Z)))
						{
							return;
						}
					}
				}
			}
		}
	}

	FORCEINLINE FVoxelIntBox& operator+=(const FVoxelIntBox& Other)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Other.Min);
		Max = FVoxelUtilities::ComponentMax(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelIntBox& operator+=(const FIntVector& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Point);
		Max = FVoxelUtilities::ComponentMax(Max, Point + 1);
		return *this;
	}
	FORCEINLINE FVoxelIntBox& operator+=(const FVector3f& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, FVoxelUtilities::FloorToInt(Point));
		Max = FVoxelUtilities::ComponentMax(Max, FVoxelUtilities::CeilToInt(Point) + 1);
		return *this;
	}
	FORCEINLINE FVoxelIntBox& operator+=(const FVector3d& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, FVoxelUtilities::FloorToInt(Point));
		Max = FVoxelUtilities::ComponentMax(Max, FVoxelUtilities::CeilToInt(Point) + 1);
		return *this;
	}
};

FORCEINLINE FVoxelIntBox operator*(const FVoxelIntBox& Box, const int32 Scale)
{
	FVoxelIntBox Copy = Box;
	return Copy *= Scale;
}

FORCEINLINE FVoxelIntBox operator*(const int32 Scale, const FVoxelIntBox& Box)
{
	FVoxelIntBox Copy = Box;
	return Copy *= Scale;
}

FORCEINLINE FVoxelIntBox operator+(const FVoxelIntBox& Box, const FVoxelIntBox& Other)
{
	return FVoxelIntBox(Box) += Other;
}

FORCEINLINE FVoxelIntBox operator+(const FVoxelIntBox& Box, const FIntVector& Point)
{
	return FVoxelIntBox(Box) += Point;
}

FORCEINLINE FVoxelIntBox operator+(const FVoxelIntBox& Box, const FVector3f& Point)
{
	return Box + FVoxelIntBox(Point);
}

FORCEINLINE FVoxelIntBox operator+(const FVoxelIntBox& Box, const FVector3d& Point)
{
	return Box + FVoxelIntBox(Point);
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelIntBox& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}

// Voxel Int Box with a IsValid flag
struct FVoxelOptionalIntBox
{
public:
	FVoxelOptionalIntBox() = default;
	FVoxelOptionalIntBox(const FVoxelIntBox& Box)
		: Box(Box)
		, bValid(true)
	{
	}

public:
	FORCEINLINE const FVoxelIntBox& GetBox() const
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

public:
	FORCEINLINE operator bool() const
	{
		return IsValid();
	}
	FORCEINLINE const FVoxelIntBox* operator->() const
	{
		return &Box;
	}
	FORCEINLINE const FVoxelIntBox& operator*() const
	{
		return Box;
	}

	FORCEINLINE FVoxelOptionalIntBox& operator=(const FVoxelIntBox& Other)
	{
		Box = Other;
		bValid = true;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelOptionalIntBox& Other) const
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

public:
	FORCEINLINE FVoxelOptionalIntBox& operator+=(const FVoxelIntBox& Other)
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
	FORCEINLINE FVoxelOptionalIntBox& operator+=(const FVoxelOptionalIntBox& Other)
	{
		if (Other.bValid)
		{
			*this += Other.GetBox();
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox& operator+=(const FIntVector& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox& operator+=(const FVector3f& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox& operator+=(const FVector3d& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox(Point);
			bValid = true;
		}
		return *this;
	}

	template<typename T>
	FORCEINLINE FVoxelOptionalIntBox& operator+=(const TArray<T>& Other)
	{
		for (auto& It : Other)
		{
			*this += It;
		}
		return *this;
	}

private:
	FVoxelIntBox Box;
	bool bValid = false;
};

template<typename T>
FORCEINLINE FVoxelOptionalIntBox operator+(const FVoxelOptionalIntBox& Box, const T& Other)
{
	FVoxelOptionalIntBox Copy = Box;
	return Copy += Other;
}