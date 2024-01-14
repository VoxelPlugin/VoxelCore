// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelTriangleTracer
{
public:
	FVoxelTriangleTracer(
		const FVector3f& VertexA,
		const FVector3f& VertexB,
		const FVector3f& VertexC)
	{
		Origin = VertexA;
		Edge1 = VertexB - VertexA;
		Edge2 = VertexC - VertexA;
		Normal = FVector3f::CrossProduct(Edge1, Edge2);
	}

private:
	FVector3f Origin;
	FVector3f Edge1;
	FVector3f Edge2;
	FVector3f Normal;

public:
	FORCEINLINE bool Trace(
		const FVector3f& RayOrigin,
		const FVector3f& RayDirection,
		const bool bAllowNegativeTime,
		float& OutTime,
		FVector3f* OutBarycentrics = nullptr) const
	{
		const FVector3f Diff = RayOrigin - Origin;

		// With:
		// Q = Diff, D = RayDirection, E1 = Edge1, E2 = Edge2, N = Cross(E1, E2)
		//
		// Solve:
		// Q + t * D = b1 * E1 + b2 * E2
		//
		// Using:
		//   |Dot(D, N)| * b1 = sign(Dot(D, N)) * Dot(D, Cross(Q, E2))
		//   |Dot(D, N)| * b2 = sign(Dot(D, N)) * Dot(D, Cross(E1, Q))
		//   |Dot(D, N)| * t = -sign(Dot(D, N)) * Dot(Q, N)

		float Dot = RayDirection.Dot(Normal);
		float Sign;
		if (Dot > KINDA_SMALL_NUMBER)
		{
			Sign = 1;
		}
		else if (Dot < -KINDA_SMALL_NUMBER)
		{
			Sign = -1;
			Dot = -Dot;
		}
		else
		{
			// Ray and triangle are parallel
			return false;
		}

		const float DotTimesB1 = Sign * RayDirection.Dot(Diff.Cross(Edge2));
		if (DotTimesB1 < 0)
		{
			// b1 < 0, no intersection
			return false;
		}

		const float DotTimesB2 = Sign * RayDirection.Dot(Edge1.Cross(Diff));
		if (DotTimesB2 < 0)
		{
			// b2 < 0, no intersection
			return false;
		}

		if (DotTimesB1 + DotTimesB2 > Dot)
		{
			// b1 + b2 > 1, no intersection
			return false;
		}

		// Line intersects triangle, check if ray does.
		const float DotTimesT = -Sign * Diff.Dot(Normal);
		if (DotTimesT < 0 && !bAllowNegativeTime)
		{
			// t < 0, no intersection
			return false;
		}

		// Ray intersects triangle.
		OutTime = DotTimesT / Dot;

		if (OutBarycentrics)
		{
			OutBarycentrics->Y = DotTimesB1 / Dot;
			OutBarycentrics->Z = DotTimesB2 / Dot;
			OutBarycentrics->X = 1 - OutBarycentrics->Y - OutBarycentrics->Z;
		}

		return true;
	}
	template<EVoxelAxis Axis>
	FORCEINLINE bool TraceAxis(
		const FVector3f& RayOrigin,
		const bool bAllowNegativeTime,
		float& OutTime,
		FVector3f* OutBarycentrics = nullptr) const
	{
		constexpr int32 AxisIndex = int32(Axis);
		const FVector3f Diff = RayOrigin - Origin;

		float Dot = Normal[AxisIndex];
		float Sign;
		if (Dot > KINDA_SMALL_NUMBER)
		{
			Sign = 1;
		}
		else if (Dot < -KINDA_SMALL_NUMBER)
		{
			Sign = -1;
			Dot = -Dot;
		}
		else
		{
			// Ray and triangle are parallel
			return false;
		}

		const float DotTimesB1 = Sign * Diff.Cross(Edge2)[AxisIndex];
		if (DotTimesB1 < 0)
		{
			// b1 < 0, no intersection
			return false;
		}

		const float DotTimesB2 = Sign * Edge1.Cross(Diff)[AxisIndex];
		if (DotTimesB2 < 0)
		{
			// b2 < 0, no intersection
			return false;
		}

		if (DotTimesB1 + DotTimesB2 > Dot)
		{
			// b1 + b2 > 1, no intersection
			return false;
		}

		// Line intersects triangle, check if ray does.
		const float DotTimesT = -Sign * Diff.Dot(Normal);
		if (DotTimesT < 0 && !bAllowNegativeTime)
		{
			// t < 0, no intersection
			return false;
		}

		// Ray intersects triangle.
		OutTime = DotTimesT / Dot;

		if (OutBarycentrics)
		{
			OutBarycentrics->Y = DotTimesB1 / Dot;
			OutBarycentrics->Z = DotTimesB2 / Dot;
			OutBarycentrics->X = 1 - OutBarycentrics->Y - OutBarycentrics->Z;
		}

		return true;
	}
};