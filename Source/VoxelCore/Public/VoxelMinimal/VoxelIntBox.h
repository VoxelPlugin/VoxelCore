// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Utilities/VoxelVectorUtilities.h"
#include "VoxelMinimal/Utilities/VoxelIntVectorUtilities.h"
#include "VoxelIntBox.generated.h"

/**
 * A Box with int32 coordinates
 */
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

	template<typename T>
	explicit FVoxelIntBox(const TArray<T>& Data)
	{
		if (!ensure(Data.Num() > 0))
		{
			Min = Max = FIntVector::ZeroValue;
			return;
		}

		*this = FVoxelIntBox(Data[0]);
		for (int32 Index = 1; Index < Data.Num(); Index++)
		{
			*this = *this + Data[Index];
		}
	}

	static FVoxelIntBox FromPositions(TConstVoxelArrayView<FIntVector> Positions);

	static FVoxelIntBox FromPositions(
		TConstVoxelArrayView<int32> PositionX,
		TConstVoxelArrayView<int32> PositionY,
		TConstVoxelArrayView<int32> PositionZ);

	FORCEINLINE static FVoxelIntBox SafeConstruct(const FIntVector& A, const FIntVector& B)
	{
		FVoxelIntBox Box;
		Box.Min = FVoxelUtilities::ComponentMin(A, B);
		Box.Max = FVoxelUtilities::ComponentMax3(A, B, Box.Min + FIntVector(1, 1, 1));
		return Box;
	}
	FORCEINLINE static FVoxelIntBox SafeConstruct(const FVector& A, const FVector& B)
	{
		FVoxelIntBox Box;
		Box.Min = FVoxelUtilities::FloorToInt(FVoxelUtilities::ComponentMin(A, B));
		Box.Max = FVoxelUtilities::CeilToInt(FVoxelUtilities::ComponentMax3(A, B, FVector(Box.Min + FIntVector(1, 1, 1))));
		return Box;
	}

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
		ensure(SizeIs32Bit());
		return Max - Min;
	}
	FORCEINLINE FVector GetCenter() const
	{
		return FVector(Min + Max) / 2.f;
	}
	FORCEINLINE FIntVector GetIntCenter() const
	{
		return (Min + Max) / 2;
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
		checkVoxelSlow(uint64(Max.X - Min.X) < (1llu << 21));
		checkVoxelSlow(uint64(Max.Y - Min.Y) < (1llu << 21));
		checkVoxelSlow(uint64(Max.Z - Min.Z) < (1llu << 21));

		return
			uint64(Max.X - Min.X) *
			uint64(Max.Y - Min.Y) *
			uint64(Max.Z - Min.Z);
	}
	FORCEINLINE int32 Count_int32() const
	{
		checkVoxelSlow(Count_uint64() <= MAX_int32);
		return int32(Count_uint64());
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

	/**
	 * Get the corners that are inside the box (max - 1)
	 */
	TVoxelStaticArray<FIntVector, 8> GetCorners(const int32 MaxBorderSize) const
	{
		return {
			FIntVector(Min.X, Min.Y, Min.Z),
			FIntVector(Max.X - MaxBorderSize, Min.Y, Min.Z),
			FIntVector(Min.X, Max.Y - MaxBorderSize, Min.Z),
			FIntVector(Max.X - MaxBorderSize, Max.Y - MaxBorderSize, Min.Z),
			FIntVector(Min.X, Min.Y, Max.Z - MaxBorderSize),
			FIntVector(Max.X - MaxBorderSize, Min.Y, Max.Z - MaxBorderSize),
			FIntVector(Min.X, Max.Y - MaxBorderSize, Max.Z - MaxBorderSize),
			FIntVector(Max.X - MaxBorderSize, Max.Y - MaxBorderSize, Max.Z - MaxBorderSize)
		};
	}
	FString ToString() const
	{
		return FString::Printf(TEXT("(%d/%d, %d/%d, %d/%d)"), Min.X, Max.X, Min.Y, Max.Y, Min.Z, Max.Z);
	}
	FORCEINLINE FVoxelBox ToVoxelBox() const
	{
		return FVoxelBox(Min, Max);
	}
	FORCEINLINE FVoxelBox ToVoxelBox_NoPadding() const
	{
		return FVoxelBox(Min, Max - 1);
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

	template<typename T>
	FORCEINLINE bool ContainsTemplate(T X, T Y, T Z) const
	{
		return ((X >= Min.X) && (X < Max.X) && (Y >= Min.Y) && (Y < Max.Y) && (Z >= Min.Z) && (Z < Max.Z));
	}
	template<typename T>
	FORCEINLINE std::enable_if_t<std::is_same_v<T, FVector3f> || std::is_same_v<T, FVector3d> || std::is_same_v<T, FIntVector>, bool> ContainsTemplate(const T& V) const
	{
		return ContainsTemplate(V.X, V.Y, V.Z);
	}
	template<typename T>
	FORCEINLINE std::enable_if_t<std::is_same_v<T, FBox> || std::is_same_v<T, FVoxelIntBox>, bool> ContainsTemplate(const T& Other) const
	{
		return Min.X <= Other.Min.X && Min.Y <= Other.Min.Y && Min.Z <= Other.Min.Z &&
			   Max.X >= Other.Max.X && Max.Y >= Other.Max.Y && Max.Z >= Other.Max.Z;
	}

	FORCEINLINE bool Contains(const int32 X, const int32 Y, const int32 Z) const
	{
		return ContainsTemplate(X, Y, Z);
	}
	FORCEINLINE bool Contains(const FIntVector& V) const
	{
		return ContainsTemplate(V);
	}
	FORCEINLINE bool Contains(const FVoxelIntBox& Other) const
	{
		return ContainsTemplate(Other);
	}

	// Not an overload as the float behavior can be a bit tricky. Use ContainsTemplate if the input type is unknown
	FORCEINLINE bool ContainsFloat(const float X, const float Y, const float Z) const
	{
		return ContainsTemplate(X, Y, Z);
	}
	FORCEINLINE bool ContainsFloat(const FVector3f& V) const
	{
		return ContainsTemplate(V);
	}
	FORCEINLINE bool ContainsFloat(const FVector3d& V) const
	{
		return ContainsTemplate(V);
	}
	FORCEINLINE bool ContainsFloat(const FBox& Other) const
	{
		return ContainsTemplate(Other);
	}

	template<typename T>
	bool Contains(T X, T Y, T Z) const = delete;

	template<typename T>
	FORCEINLINE T Clamp(T P, const int32 Step = 1) const
	{
		Clamp(P.X, P.Y, P.Z, Step);
		return P;
	}
	FORCEINLINE void Clamp(int32& X, int32& Y, int32& Z, const int32 Step = 1) const
	{
		X = FMath::Clamp(X, Min.X, Max.X - Step);
		Y = FMath::Clamp(Y, Min.Y, Max.Y - Step);
		Z = FMath::Clamp(Z, Min.Z, Max.Z - Step);
		ensureVoxelSlowNoSideEffects(Contains(X, Y, Z));
	}
	template<typename T>
	FORCEINLINE void Clamp(T& X, T& Y, T& Z) const
	{
		// Note: use - 1 even if that's not the closest value for which Contains would return true
		// because it's really hard to figure out that value (largest float f such that f < i)
		X = FMath::Clamp<T>(X, Min.X, Max.X - 1);
		Y = FMath::Clamp<T>(Y, Min.Y, Max.Y - 1);
		Z = FMath::Clamp<T>(Z, Min.Z, Max.Z - 1);
		ensureVoxelSlowNoSideEffects(ContainsTemplate(X, Y, Z));
	}
	FORCEINLINE FVoxelIntBox Clamp(const FVoxelIntBox& Other) const
	{
		// It's not valid to call Clamp if we're not intersecting Other
		ensureVoxelSlowNoSideEffects(Intersect(Other));

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

	template<typename TBox>
	FORCEINLINE bool Intersect(const TBox& Other) const
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
	/**
	 * Useful for templates taking a box or coordinates
	 */
	template<typename TNumeric>
	FORCEINLINE bool Intersect(TNumeric X, TNumeric Y, TNumeric Z) const
	{
		return ContainsTemplate(X, Y, Z);
	}

	/**
	 * Useful for templates taking a box or coordinates
	 */
	template<typename TNumeric, typename TVector>
	FORCEINLINE bool IntersectSphere(TVector Center, TNumeric Radius) const
	{
		// See FMath::SphereAABBIntersection
		return ComputeSquaredDistanceFromBoxToPoint<TNumeric>(Center) <= FMath::Square(Radius);
	}

	// Return the intersection of the two boxes
	FVoxelIntBox Overlap(const FVoxelIntBox& Other) const
	{
		if (!Intersect(Other))
		{
			return FVoxelIntBox();
		}

		// otherwise they overlap
		// so find overlapping box
		FIntVector MinVector, MaxVector;

		MinVector.X = FMath::Max(Min.X, Other.Min.X);
		MaxVector.X = FMath::Min(Max.X, Other.Max.X);

		MinVector.Y = FMath::Max(Min.Y, Other.Min.Y);
		MaxVector.Y = FMath::Min(Max.Y, Other.Max.Y);

		MinVector.Z = FMath::Max(Min.Z, Other.Min.Z);
		MaxVector.Z = FMath::Min(Max.Z, Other.Max.Z);

		return FVoxelIntBox(MinVector, MaxVector);
	}
	FVoxelIntBox Union(const FVoxelIntBox& Other) const
	{
		FIntVector MinVector, MaxVector;

		MinVector.X = FMath::Min(Min.X, Other.Min.X);
		MaxVector.X = FMath::Max(Max.X, Other.Max.X);

		MinVector.Y = FMath::Min(Min.Y, Other.Min.Y);
		MaxVector.Y = FMath::Max(Max.Y, Other.Max.Y);

		MinVector.Z = FMath::Min(Min.Z, Other.Min.Z);
		MaxVector.Z = FMath::Max(Max.Z, Other.Max.Z);

		return FVoxelIntBox(MinVector, MaxVector);
	}

	// union(return value, Other) = this
	TArray<FVoxelIntBox, TFixedAllocator<6>> Difference(const FVoxelIntBox& Other) const;

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType ComputeSquaredDistanceFromBoxToPoint(const VectorType& Point) const
	{
		// Accumulates the distance as we iterate axis
		ScalarType DistSquared = 0;

		// Check each axis for min/max and add the distance accordingly
		if (Point.X < Min.X)
		{
			DistSquared += FMath::Square<ScalarType>(Min.X - Point.X);
		}
		else if (Point.X > Max.X)
		{
			DistSquared += FMath::Square<ScalarType>(Point.X - Max.X);
		}

		if (Point.Y < Min.Y)
		{
			DistSquared += FMath::Square<ScalarType>(Min.Y - Point.Y);
		}
		else if (Point.Y > Max.Y)
		{
			DistSquared += FMath::Square<ScalarType>(Point.Y - Max.Y);
		}

		if (Point.Z < Min.Z)
		{
			DistSquared += FMath::Square<ScalarType>(Min.Z - Point.Z);
		}
		else if (Point.Z > Max.Z)
		{
			DistSquared += FMath::Square<ScalarType>(Point.Z - Max.Z);
		}

		return DistSquared;
	}
	FORCEINLINE uint64 ComputeSquaredDistanceFromBoxToPoint(const FIntVector& Point) const
	{
		return ComputeSquaredDistanceFromBoxToPoint<FIntVector, uint64>(Point);
	}
	FORCEINLINE double DistanceFromBoxToPoint(const FVector3d& Point) const
	{
		return FMath::Sqrt(ComputeSquaredDistanceFromBoxToPoint(Point));
	}

	// We try to make the following true:
	// Box.ApproximateDistanceToBox(Box.ShiftBy(X)) == X
	float ApproximateDistanceToBox(const FVoxelIntBox& Other) const
	{
		return (FVoxelUtilities::Size(Min - Other.Min) + FVoxelUtilities::Size(Max - Other.Max)) / 2;
	}

	FORCEINLINE bool IsMultipleOf(const int32 Step) const
	{
		return Min.X % Step == 0 && Min.Y % Step == 0 && Min.Z % Step == 0 &&
			   Max.X % Step == 0 && Max.Y % Step == 0 && Max.Z % Step == 0;
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

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<std::is_void_v<ReturnType> || std::is_same_v<ReturnType, bool>>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType(const FVoxelIntBox&)>>
	void IterateChunks(const int32 ChunkSize, LambdaType&& Lambda) const
	{
		const FIntVector KeyMin = FVoxelUtilities::DivideFloor(Min, ChunkSize);
		const FIntVector KeyMax = FVoxelUtilities::DivideCeil(Max, ChunkSize);

		for (int32 X = KeyMin.X; X < KeyMax.X; X++)
		{
			for (int32 Y = KeyMin.Y; Y < KeyMax.Y; Y++)
			{
				for (int32 Z = KeyMin.Z; Z < KeyMax.Z; Z++)
				{
					FVoxelIntBox Chunk(
						FIntVector(ChunkSize * (X + 0), ChunkSize * (Y + 0), ChunkSize * (Z + 0)),
						FIntVector(ChunkSize * (X + 1), ChunkSize * (Y + 1), ChunkSize * (Z + 1)));

					Chunk = Chunk.Overlap(*this);

					if constexpr (std::is_void_v<ReturnType>)
					{
						Lambda(Chunk);
					}
					else
					{
						if (!Lambda(Chunk))
						{
							return;
						}
					}
				}
			}
		}
	}

	FORCEINLINE FVoxelIntBox Scale(float S) const = delete;
	FORCEINLINE FVoxelIntBox Scale(const int32 S) const
	{
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
	// Will move the box so that GetCenter = 0,0,0. Will extend it if its size is odd
	FVoxelIntBox Center() const
	{
		FIntVector NewMin = Min;
		FIntVector NewMax = Max;
		if (FVector(FIntVector(GetCenter())) != GetCenter())
		{
			NewMax = NewMax + 1;
		}
		ensure(FVector(FIntVector(GetCenter())) == GetCenter());
		const FIntVector Offset = FIntVector(GetCenter());
		NewMin -= Offset;
		NewMax -= Offset;
		ensure(NewMin + NewMax == FIntVector(0));
		return FVoxelIntBox(NewMin, NewMax);
	}

	FORCEINLINE FVoxelIntBox& operator*=(const int32 Scale)
	{
		Min *= Scale;
		Max *= Scale;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelIntBox& Other) const
	{
		return Min == Other.Min && Max == Other.Max;
	}
	FORCEINLINE bool operator!=(const FVoxelIntBox& Other) const
	{
		return Min != Other.Min || Max != Other.Max;
	}

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<std::is_void_v<ReturnType> || std::is_same_v<ReturnType, bool>>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType(const FIntVector&)>>
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

	// MaxBorderSize: if we do a 180 rotation for example, min and max are inverted
	// If we don't work on values that are actually inside the box, the resulting box will be wrong
	FVoxelIntBox ApplyTransform(const FMatrix44f& Transform, const int32 MaxBorderSize = 1) const
	{
		return ApplyTransformImpl([&](const FIntVector& Position)
		{
			return Transform.TransformPosition(FVector3f(Position));
		}, MaxBorderSize);
	}
	FVoxelIntBox ApplyTransform(const FTransform3f& Transform, const int32 MaxBorderSize = 1) const
	{
		return ApplyTransformImpl([&](const FIntVector& Position)
		{
			return Transform.TransformPosition(FVector3f(Position));
		}, MaxBorderSize);
	}

	FVoxelIntBox ApplyTransform(const FMatrix44d& Transform, const int32 MaxBorderSize = 1) const
	{
		return ApplyTransformImpl([&](const FIntVector& Position)
		{
			return Transform.TransformPosition(FVector3d(Position));
		}, MaxBorderSize);
	}
	FVoxelIntBox ApplyTransform(const FTransform3d& Transform, const int32 MaxBorderSize = 1) const
	{
		return ApplyTransformImpl([&](const FIntVector& Position)
		{
			return Transform.TransformPosition(FVector3d(Position));
		}, MaxBorderSize);
	}

	template<typename T>
	FVoxelIntBox ApplyTransformImpl(T GetNewPosition, const int32 MaxBorderSize = 1) const
	{
		const auto Corners = GetCorners(MaxBorderSize);

		FIntVector NewMin(MAX_int32);
		FIntVector NewMax(MIN_int32);
		for (int32 Index = 0; Index < 8; Index++)
		{
			const FVector P = FVector(GetNewPosition(Corners[Index]));
			NewMin = FVoxelUtilities::ComponentMin(NewMin, FVoxelUtilities::FloorToInt(P));
			NewMax = FVoxelUtilities::ComponentMax(NewMax, FVoxelUtilities::CeilToInt(P));
		}
		return FVoxelIntBox(NewMin, NewMax + MaxBorderSize);
	}
	template<typename T>
	FBox ApplyTransformFloatImpl(T GetNewPosition, const int32 MaxBorderSize = 1) const
	{
		const auto Corners = GetCorners(MaxBorderSize);

		FVector NewMin = GetNewPosition(Corners[0]);
		FVector NewMax = NewMin;
		for (int32 Index = 1; Index < 8; Index++)
		{
			const FVector P = GetNewPosition(Corners[Index]);
			NewMin = FVoxelUtilities::ComponentMin(NewMin, P);
			NewMax = FVoxelUtilities::ComponentMax(NewMax, P);
		}
		return FBox(NewMin, NewMax + MaxBorderSize);
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
	FVoxelOptionalIntBox() = default;
	FVoxelOptionalIntBox(const FVoxelIntBox& Box)
		: Box(Box)
		, bValid(true)
	{
	}

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
	FORCEINLINE bool operator!=(const FVoxelOptionalIntBox& Other) const
	{
		return !(*this == Other);
	}

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