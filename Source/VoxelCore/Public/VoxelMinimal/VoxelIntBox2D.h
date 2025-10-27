// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox2D.h"
#include "VoxelMinimal/VoxelIntBox.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"
#include "VoxelMinimal/Utilities/VoxelIntPointUtilities.h"
#include "VoxelIntBox2D.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelIntBox2D
{
	GENERATED_BODY()

	// Inclusive
	UPROPERTY()
	FIntPoint Min = FIntPoint(ForceInit);

	// Exclusive
	UPROPERTY()
	FIntPoint Max = FIntPoint(ForceInit);

	static const FVoxelIntBox2D Infinite;
	static const FVoxelIntBox2D InvertedInfinite;

	FVoxelIntBox2D() = default;

	FORCEINLINE FVoxelIntBox2D(const FIntPoint& Min, const FIntPoint& Max)
		: Min(Min)
		, Max(Max)
	{
		ensureVoxelSlow(Min.X <= Max.X);
		ensureVoxelSlow(Min.Y <= Max.Y);
	}
	FORCEINLINE explicit FVoxelIntBox2D(const int32 Min, const FIntPoint& Max)
		: FVoxelIntBox2D(FIntPoint(Min), Max)
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const FIntPoint& Min, const int32 Max)
		: FVoxelIntBox2D(Min, FIntPoint(Max))
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const int32 Min, const int32 Max)
		: FVoxelIntBox2D(FIntPoint(Min), FIntPoint(Max))
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const FVector2f& Min, const FVector2f& Max)
		: FVoxelIntBox2D(FVoxelUtilities::FloorToInt(Min), FVoxelUtilities::CeilToInt(Max) + 1)
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const FVector2d& Min, const FVector2d& Max)
		: FVoxelIntBox2D(FVoxelUtilities::FloorToInt(Min), FVoxelUtilities::CeilToInt(Max) + 1)
	{
	}

	FORCEINLINE explicit FVoxelIntBox2D(const FVector2f& Position)
		: FVoxelIntBox2D(Position, Position)
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const FVector2d& Position)
		: FVoxelIntBox2D(Position, Position)
	{
	}

	FORCEINLINE explicit FVoxelIntBox2D(const FIntPoint& Position)
		: FVoxelIntBox2D(Position, Position + 1)
	{
	}
	FORCEINLINE explicit FVoxelIntBox2D(const FVoxelIntBox& Box)
		: FVoxelIntBox2D(FIntPoint(Box.Min.X, Box.Min.Y), FIntPoint(Box.Max.X, Box.Max.Y))
	{
	}

	static FVoxelIntBox2D FromPositions(TConstVoxelArrayView<FIntPoint> Positions);

	static FVoxelIntBox2D FromPositions(
		TConstVoxelArrayView<int32> PositionX,
		TConstVoxelArrayView<int32> PositionY);

	template<typename T>
	FORCEINLINE static FVoxelIntBox2D FromFloatBox_NoPadding(T Box)
	{
		return
		{
			FVoxelUtilities::FloorToInt(Box.Min),
			FVoxelUtilities::CeilToInt(Box.Max),
		};
	}
	template<typename T>
	FORCEINLINE static FVoxelIntBox2D FromFloatBox_WithPadding(T Box)
	{
		return
		{
			FVoxelUtilities::FloorToInt(Box.Min),
			FVoxelUtilities::CeilToInt(Box.Max) + 1,
		};
	}

	FORCEINLINE FIntPoint Size() const
	{
		checkVoxelSlow(SizeIs32Bit());
		return Max - Min;
	}
	FORCEINLINE FVector2D GetCenter() const
	{
		return FVector2D(Min + Max) / 2.f;
	}
	FORCEINLINE FIntPoint GetIntCenter() const
	{
		return (Min + Max) / 2;
	}
	FORCEINLINE double Count_double() const
	{
		return
			(double(Max.X) - double(Min.X)) *
			(double(Max.Y) - double(Min.Y));
	}
	FORCEINLINE uint64 Count_uint64() const
	{
		checkVoxelSlow(int64(Max.X) - int64(Min.X) < (1llu << 32));
		checkVoxelSlow(int64(Max.Y) - int64(Min.Y) < (1llu << 32));

		return
			uint64(Max.X - Min.X) *
			uint64(Max.Y - Min.Y);
	}
	FORCEINLINE int32 Count_int32() const
	{
		checkVoxelSlow(Count_uint64() <= MAX_int32);

		return
			int32(Max.X - Min.X) *
			int32(Max.Y - Min.Y);
	}

	FORCEINLINE bool SizeIs32Bit() const
	{
		return
			int64(Max.X) - int64(Min.X) < MAX_int32 &&
			int64(Max.Y) - int64(Min.Y) < MAX_int32;
	}

	FORCEINLINE bool IsInfinite() const
	{
		// Not exactly accurate, but should be safe
		const int32 InfiniteMin = MIN_int32 / 2;
		const int32 InfiniteMax = MAX_int32 / 2;
		return
			Min.X < InfiniteMin ||
			Min.Y < InfiniteMin ||
			Max.X > InfiniteMax ||
			Max.Y > InfiniteMax;
	}

	FString ToString() const;

	FORCEINLINE FVoxelBox2D ToVoxelBox2D() const
	{
		return FVoxelBox2D(Min, Max);
	}
	FORCEINLINE FBox2D ToFBox2D() const
	{
		return FBox2D(FVector2D(Min), FVector2D(Max));
	}
	FORCEINLINE FBox2f ToFBox2f() const
	{
		return FBox2f(FVector2f(Min), FVector2f(Max));
	}

	FORCEINLINE FVoxelIntInterval GetX() const
	{
		return { Min.X, Max.X };
	}
	FORCEINLINE FVoxelIntInterval GetY() const
	{
		return { Min.Y, Max.Y };
	}

	FORCEINLINE bool IsValid() const
	{
		return
			Min.X < Max.X &&
			Min.Y < Max.Y;
	}

	FORCEINLINE bool Contains(const int32 X, const int32 Y) const
	{
		return
			(X >= Min.X) && (X < Max.X) &&
			(Y >= Min.Y) && (Y < Max.Y);
	}
	FORCEINLINE bool Contains(const FIntPoint& Point) const
	{
		return
			(Point.X >= Min.X) && (Point.X < Max.X) &&
			(Point.Y >= Min.Y) && (Point.Y < Max.Y);
	}
	FORCEINLINE bool Contains(const FVoxelIntBox2D& Other) const
	{
		return
			Min.X <= Other.Min.X &&
			Min.Y <= Other.Min.Y &&
			Max.X >= Other.Max.X &&
			Max.Y >= Other.Max.Y;
	}

	template<typename T>
	bool Contains(T X, T Y) const = delete;

	FORCEINLINE FIntPoint Clamp(FIntPoint P, const int32 Step = 1) const
	{
		Clamp(P.X, P.Y, Step);
		return P;
	}
	FORCEINLINE void Clamp(int32& X, int32& Y, const int32 Step = 1) const
	{
		X = FMath::Clamp(X, Min.X, Max.X - Step);
		Y = FMath::Clamp(Y, Min.Y, Max.Y - Step);
		ensureVoxelSlowNoSideEffects(Contains(X, Y));
	}
	FORCEINLINE FVoxelIntBox2D Clamp(const FVoxelIntBox2D& Other) const
	{
		// It's not valid to call Clamp if we're not intersecting Other
		ensureVoxelSlowNoSideEffects(Intersects(Other));

		FVoxelIntBox2D Result;

		Result.Min.X = FMath::Clamp(Other.Min.X, Min.X, Max.X - 1);
		Result.Min.Y = FMath::Clamp(Other.Min.Y, Min.Y, Max.Y - 1);

		Result.Max.X = FMath::Clamp(Other.Max.X, Min.X + 1, Max.X);
		Result.Max.Y = FMath::Clamp(Other.Max.Y, Min.Y + 1, Max.Y);

		ensureVoxelSlowNoSideEffects(Other.Contains(Result));
		return Result;
	}

	FORCEINLINE bool Intersects(const FVoxelIntBox2D& Other) const
	{
		if ((Min.X >= Other.Max.X) || (Other.Min.X >= Max.X))
		{
			return false;
		}

		if ((Min.Y >= Other.Max.Y) || (Other.Min.Y >= Max.Y))
		{
			return false;
		}

		return true;
	}

	FORCEINLINE FVoxelIntBox2D IntersectWith(const FVoxelIntBox2D& Other) const
	{
		const FIntPoint NewMin = FVoxelUtilities::ComponentMax(Min, Other.Min);
		const FIntPoint NewMax = FVoxelUtilities::ComponentMin(Max, Other.Max);

		if (NewMin.X >= NewMax.X ||
			NewMin.Y >= NewMax.Y)
		{
			return {};
		}

		return FVoxelIntBox2D(NewMin, NewMax);
	}
	FORCEINLINE FVoxelIntBox2D UnionWith(const FVoxelIntBox2D& Other) const
	{
		return FVoxelIntBox2D(
			FVoxelUtilities::ComponentMin(Min, Other.Min),
			FVoxelUtilities::ComponentMax(Max, Other.Max));
	}

	FVoxelIntBox2D Remove_Union(const FVoxelIntBox2D& Other) const;

	void Remove_Split(
		const FVoxelIntBox2D& Other,
		TVoxelArray<FVoxelIntBox2D>& OutRemainder) const;

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
	FORCEINLINE uint64 SquaredDistanceToPoint(const FIntPoint& Point) const
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

		return DistSquared;
	}
	FORCEINLINE double DistanceToPoint(const FVector2D& Point) const
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
	FORCEINLINE FVoxelIntBox2D MakeMultipleOfBigger(const int32 Step) const
	{
		FVoxelIntBox2D NewBox;
		NewBox.Min = FVoxelUtilities::DivideFloor(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step) * Step;
		return NewBox;
	}
	// NewBox included in OldBox, but OldBox not included in NewBox
	FORCEINLINE FVoxelIntBox2D MakeMultipleOfSmaller(const int32 Step) const
	{
		FVoxelIntBox2D NewBox;
		NewBox.Min = FVoxelUtilities::DivideCeil(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideFloor(Max, Step) * Step;
		return NewBox;
	}
	FORCEINLINE FVoxelIntBox2D MakeMultipleOfRoundUp(const int32 Step) const
	{
		FVoxelIntBox2D NewBox;
		NewBox.Min = FVoxelUtilities::DivideCeil(Min, Step) * Step;
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step) * Step;
		return NewBox;
	}

	FORCEINLINE FVoxelIntBox2D DivideBigger(const int32 Step) const
	{
		FVoxelIntBox2D NewBox;
		NewBox.Min = FVoxelUtilities::DivideFloor(Min, Step);
		NewBox.Max = FVoxelUtilities::DivideCeil(Max, Step);
		return NewBox;
	}
	FORCEINLINE FVoxelIntBox2D DivideExact(const int32 Step) const
	{
		checkVoxelSlow(Min % Step == 0);
		checkVoxelSlow(Max % Step == 0);

		FVoxelIntBox2D NewBox;
		NewBox.Min = Min / Step;
		NewBox.Max = Max / Step;
		return NewBox;
	}

	// Guarantee: union(OutChilds).Contains(this)
	bool Subdivide(
		int32 ChildrenSize,
		TVoxelArray<FVoxelIntBox2D>& OutChildren,
		bool bUseOverlap,
		int32 MaxChildren = -1) const;

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterate>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FVoxelIntBox2D&)>
	)
	void IterateChunks(const int32 ChunkSize, LambdaType&& Lambda) const
	{
		const FIntPoint KeyMin = FVoxelUtilities::DivideFloor(Min, ChunkSize);
		const FIntPoint KeyMax = FVoxelUtilities::DivideCeil(Max, ChunkSize);

		for (int32 Y = KeyMin.Y; Y < KeyMax.Y; Y++)
		{
			for (int32 X = KeyMin.X; X < KeyMax.X; X++)
			{
				FVoxelIntBox2D Chunk(
					FIntPoint(ChunkSize * (X + 0), ChunkSize * (Y + 0)),
					FIntPoint(ChunkSize * (X + 1), ChunkSize * (Y + 1)));

				Chunk = Chunk.IntersectWith(*this);

				if constexpr (std::is_void_v<ReturnType>)
				{
					Lambda(Chunk);
				}
				else
				{
					if (Lambda(Chunk) == EVoxelIterate::Stop)
					{
						return;
					}
				}
			}
		}
	}

	FORCEINLINE FVoxelIntBox2D Scale(float S) const = delete;
	FORCEINLINE FVoxelIntBox2D Scale(const int32 S) const
	{
		ensureVoxelSlow(S >= 0);
		return { Min * S, Max * S };
	}
	FORCEINLINE FVoxelIntBox2D Scale(const FVector2D& S) const = delete;

	FORCEINLINE FVoxelIntBox2D Extend(const FIntPoint& Amount) const
	{
		return { Min - Amount, Max + Amount };
	}
	FORCEINLINE FVoxelIntBox2D Extend(const int32 Amount) const
	{
		return Extend(FIntPoint(Amount));
	}
	FORCEINLINE FVoxelIntBox2D Translate(const FIntPoint& Position) const
	{
		return FVoxelIntBox2D(Min + Position, Max + Position);
	}
	FORCEINLINE FVoxelIntBox2D ShiftBy(const FIntPoint& Offset) const
	{
		return Translate(Offset);
	}

	FORCEINLINE FVoxelIntBox2D RemoveTranslation() const
	{
		return FVoxelIntBox2D(0, Max - Min);
	}

	FORCEINLINE FVoxelIntBox2D& operator*=(const int32 S)
	{
		*this = Scale(S);
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelIntBox2D& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterate>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FIntPoint&)>
	)
	FORCEINLINE void Iterate(LambdaType&& Lambda) const
	{
		for (int32 Y = Min.Y; Y < Max.Y; Y++)
		{
			for (int32 X = Min.X; X < Max.X; X++)
			{
				if constexpr (std::is_void_v<ReturnType>)
				{
					Lambda(FIntPoint(X, Y));
				}
				else
				{
					if (Lambda(FIntPoint(X, Y)) == EVoxelIterate::Stop)
					{
						return;
					}
				}
			}
		}
	}

	FORCEINLINE FVoxelIntBox2D& operator+=(const FVoxelIntBox2D& Other)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Other.Min);
		Max = FVoxelUtilities::ComponentMax(Max, Other.Max);
		return *this;
	}
	FORCEINLINE FVoxelIntBox2D& operator+=(const FIntPoint& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, Point);
		Max = FVoxelUtilities::ComponentMax(Max, Point + 1);
		return *this;
	}
	FORCEINLINE FVoxelIntBox2D& operator+=(const FVector2f& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, FVoxelUtilities::FloorToInt(Point));
		Max = FVoxelUtilities::ComponentMax(Max, FVoxelUtilities::CeilToInt(Point) + 1);
		return *this;
	}
	FORCEINLINE FVoxelIntBox2D& operator+=(const FVector2d& Point)
	{
		Min = FVoxelUtilities::ComponentMin(Min, FVoxelUtilities::FloorToInt(Point));
		Max = FVoxelUtilities::ComponentMax(Max, FVoxelUtilities::CeilToInt(Point) + 1);
		return *this;
	}
};

FORCEINLINE FVoxelIntBox2D operator*(const FVoxelIntBox2D& Box, const int32 Scale)
{
	FVoxelIntBox2D Copy = Box;
	return Copy *= Scale;
}

FORCEINLINE FVoxelIntBox2D operator*(const int32 Scale, const FVoxelIntBox2D& Box)
{
	FVoxelIntBox2D Copy = Box;
	return Copy *= Scale;
}

FORCEINLINE FVoxelIntBox2D operator+(const FVoxelIntBox2D& Box, const FVoxelIntBox2D& Other)
{
	return FVoxelIntBox2D(Box) += Other;
}

FORCEINLINE FVoxelIntBox2D operator+(const FVoxelIntBox2D& Box, const FIntPoint& Point)
{
	return FVoxelIntBox2D(Box) += Point;
}

FORCEINLINE FVoxelIntBox2D operator+(const FVoxelIntBox2D& Box, const FVector2f& Point)
{
	return Box + FVoxelIntBox2D(Point);
}

FORCEINLINE FVoxelIntBox2D operator+(const FVoxelIntBox2D& Box, const FVector2d& Point)
{
	return Box + FVoxelIntBox2D(Point);
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, FVoxelIntBox2D& Box)
{
	Ar << Box.Min;
	Ar << Box.Max;
	return Ar;
}

// Voxel Int Box with a IsValid flag
struct FVoxelOptionalIntBox2D
{
public:
	FVoxelOptionalIntBox2D() = default;
	FVoxelOptionalIntBox2D(const FVoxelIntBox2D& Box)
		: Box(Box)
		, bValid(true)
	{
	}

public:
	FORCEINLINE const FVoxelIntBox2D& GetBox() const
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
	FORCEINLINE const FVoxelIntBox2D* operator->() const
	{
		return &Box;
	}
	FORCEINLINE const FVoxelIntBox2D& operator*() const
	{
		return Box;
	}

	FORCEINLINE FVoxelOptionalIntBox2D& operator=(const FVoxelIntBox2D& Other)
	{
		Box = Other;
		bValid = true;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelOptionalIntBox2D& Other) const
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
	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const FVoxelIntBox2D& Other)
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
	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const FVoxelOptionalIntBox2D& Other)
	{
		if (Other.bValid)
		{
			*this += Other.GetBox();
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const FIntPoint& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox2D(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const FVector2f& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox2D(Point);
			bValid = true;
		}
		return *this;
	}

	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const FVector2d& Point)
	{
		if (bValid)
		{
			Box += Point;
		}
		else
		{
			Box = FVoxelIntBox2D(Point);
			bValid = true;
		}
		return *this;
	}

	template<typename T>
	FORCEINLINE FVoxelOptionalIntBox2D& operator+=(const TArray<T>& Other)
	{
		for (auto& It : Other)
		{
			*this += It;
		}
		return *this;
	}

private:
	FVoxelIntBox2D Box;
	bool bValid = false;
};

template<typename T>
FORCEINLINE FVoxelOptionalIntBox2D operator+(const FVoxelOptionalIntBox2D& Box, const T& Other)
{
	FVoxelOptionalIntBox2D Copy = Box;
	return Copy += Other;
}