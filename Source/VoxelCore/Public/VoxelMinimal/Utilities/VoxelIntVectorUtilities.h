// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

namespace FVoxelUtilities
{
	FORCEINLINE bool CountIs32Bits(const FIntVector& Size)
	{
		return FMath::Abs(int64(Size.X) * int64(Size.Y) * int64(Size.Z)) < MAX_int32;
	}

	FORCEINLINE FIntVector DivideFloor(const FIntVector& Vector, const int32 Divisor)
	{
		return FIntVector(
			DivideFloor(Vector.X, Divisor),
			DivideFloor(Vector.Y, Divisor),
			DivideFloor(Vector.Z, Divisor));
	}
	FORCEINLINE FIntVector DivideFloor_Positive(const FIntVector& Vector, const int32 Divisor)
	{
		return FIntVector(
			DivideFloor_Positive(Vector.X, Divisor),
			DivideFloor_Positive(Vector.Y, Divisor),
			DivideFloor_Positive(Vector.Z, Divisor));
	}
	FORCEINLINE FIntVector DivideFloor_FastLog2(const FIntVector& Vector, const int32 DivisorLog2)
	{
		return FIntVector(
			DivideFloor_FastLog2(Vector.X, DivisorLog2),
			DivideFloor_FastLog2(Vector.Y, DivisorLog2),
			DivideFloor_FastLog2(Vector.Z, DivisorLog2));
	}

	FORCEINLINE FIntVector DivideCeil(const FIntVector& Vector, const int32 Divisor)
	{
		return FIntVector(
			DivideCeil(Vector.X, Divisor),
			DivideCeil(Vector.Y, Divisor),
			DivideCeil(Vector.Z, Divisor));
	}

	FORCEINLINE int64 SizeSquared(const FIntVector& Vector)
	{
		return
			FMath::Square<int64>(Vector.X) +
			FMath::Square<int64>(Vector.Y) +
			FMath::Square<int64>(Vector.Z);
	}
	FORCEINLINE double Size(const FIntVector& Vector)
	{
		return FMath::Sqrt(double(SizeSquared(Vector)));
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<>
struct TLess<FIntVector>
{
	FORCEINLINE bool operator()(const FIntVector& A, const FIntVector& B) const
	{
		if (A.X != B.X) return A.X < B.X;
		if (A.Y != B.Y) return A.Y < B.Y;
		return A.Z < B.Z;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntVector operator-(const FIntVector& Vector)
{
	return FIntVector(-Vector.X, -Vector.Y, -Vector.Z);
}
FORCEINLINE FIntVector operator-(const FIntVector& Vector, const int32 Value)
{
	return Vector - FIntVector(Value);
}
FORCEINLINE FIntVector operator-(const FIntVector& Vector, const uint32 Value)
{
	checkVoxelSlow(Value <= MAX_int32);
	return Vector - FIntVector(int32(Value));
}
FORCEINLINE FIntVector operator-(const int32 Value, const FIntVector& Vector)
{
	return FIntVector(Value) - Vector;
}
FORCEINLINE FIntVector operator-(const uint32 Value, const FIntVector& Vector)
{
	checkVoxelSlow(Value <= MAX_int32);
	return FIntVector(int32(Value)) - Vector;
}

template<typename T>
FIntVector operator-(const FIntVector& V, T A) = delete;
template<typename T>
FIntVector operator-(T A, const FIntVector& V) = delete;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntVector operator+(const FIntVector& Vector, const int32 Value)
{
	return Vector + FIntVector(Value);
}
FORCEINLINE FIntVector operator+(const FIntVector& Vector, const uint32 Value)
{
	checkVoxelSlow(Value <= MAX_int32);
	return Vector + FIntVector(int32(Value));
}
FORCEINLINE FIntVector operator+(const int32 Value, const FIntVector& Vector)
{
	return FIntVector(Value) + Vector;
}
FORCEINLINE FIntVector operator+(const uint32 Value, const FIntVector& Vector)
{
	checkVoxelSlow(Value <= MAX_int32);
	return FIntVector(int32(Value)) + Vector;
}

template<typename T>
FIntVector operator+(const FIntVector& V, T A) = delete;
template<typename T>
FIntVector operator+(T A, const FIntVector& V) = delete;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntVector operator*(const int32 Value, const FIntVector& Vector)
{
	return FIntVector(Value) * Vector;
}
FORCEINLINE FIntVector operator*(const uint32 Value, const FIntVector& Vector)
{
	checkVoxelSlow(Value <= MAX_int32);
	return FIntVector(int32(Value)) * Vector;
}
FORCEINLINE FIntVector operator*(const FIntVector& Vector, const uint32 Value)
{
	checkVoxelSlow(Value <= MAX_int32);
	return Vector * FIntVector(int32(Value));
}
FORCEINLINE FIntVector operator*(const FIntVector& A, const FIntVector& B)
{
	return FIntVector(A.X * B.X, A.Y * B.Y, A.Z * B.Z);
}

template<typename T>
FIntVector operator*(const FIntVector& V, T A) = delete;
template<typename T>
FIntVector operator*(T A, const FIntVector& V) = delete;

FORCEINLINE FVector3f operator*(const FVector3f& FloatVector, const FIntVector& IntVector)
{
	return FVector3f(
		FloatVector.X * IntVector.X,
		FloatVector.Y * IntVector.Y,
		FloatVector.Z * IntVector.Z);
}
FORCEINLINE FVector3d operator*(const FVector3d& FloatVector, const FIntVector& IntVector)
{
	return FVector3d(
		FloatVector.X * IntVector.X,
		FloatVector.Y * IntVector.Y,
		FloatVector.Z * IntVector.Z);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE bool operator==(const FIntVector& Vector, const int32 Value)
{
	return
		Vector.X == Value &&
		Vector.Y == Value &&
		Vector.Z == Value;
}

template<typename T>
FIntVector operator%(const FIntVector& V, T A) = delete;
template<typename T>
FIntVector operator/(const FIntVector& V, T A) = delete;