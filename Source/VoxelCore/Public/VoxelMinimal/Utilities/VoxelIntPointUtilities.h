// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

namespace FVoxelUtilities
{
	FORCEINLINE FIntPoint DivideFloor(const FIntPoint& V, const int32 Divisor)
	{
		return FIntPoint(
			DivideFloor(V.X, Divisor),
			DivideFloor(V.Y, Divisor));
	}
	FORCEINLINE FIntPoint DivideCeil(const FIntPoint& V, const int32 Divisor)
	{
		return FIntPoint(
			DivideCeil(V.X, Divisor),
			DivideCeil(V.Y, Divisor));
	}
	FORCEINLINE FIntPoint DivideFloor_FastLog2(const FIntPoint& Vector, const int32 DivisorLog2)
	{
		return FIntPoint(
			DivideFloor_FastLog2(Vector.X, DivisorLog2),
			DivideFloor_FastLog2(Vector.Y, DivisorLog2));
	}

	FORCEINLINE int64 SizeSquared(const FIntPoint& V)
	{
		return
			FMath::Square<int64>(V.X) +
			FMath::Square<int64>(V.Y);
	}
	FORCEINLINE double Size(const FIntPoint& V)
	{
		return FMath::Sqrt(double(SizeSquared(V)));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntPoint operator-(const FIntPoint& Vector)
{
	return FIntPoint(-Vector.X, -Vector.Y);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntPoint operator*(const int32 Value, const FIntPoint& Vector)
{
	return FIntPoint(Value) * Vector;
}
FORCEINLINE FIntPoint operator*(const uint32 Value, const FIntPoint& Vector)
{
	checkVoxelSlow(Value <= MAX_int32);
	return FIntPoint(int32(Value)) * Vector;
}

FIntPoint operator*(const FIntPoint&, float) = delete;
FIntPoint operator*(float, const FIntPoint& V) = delete;

FIntPoint operator*(const FIntPoint&, double) = delete;
FIntPoint operator*(double, const FIntPoint& V) = delete;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FIntPoint operator%(const FIntPoint& A, const FIntPoint& B)
{
	return FIntPoint(A.X % B.X, A.Y % B.Y);
}
FORCEINLINE FIntPoint operator%(const FIntPoint& Vector, const int32 Value)
{
	return Vector % FIntPoint(Value);
}
FORCEINLINE FIntPoint operator%(const FIntPoint& Vector, const uint32 Value)
{
	checkVoxelSlow(Value <= MAX_int32);
	return Vector % FIntPoint(int32(Value));
}