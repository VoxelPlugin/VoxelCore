// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE bool SegmentAreIntersecting(
		const VectorType& StartA,
		const VectorType& EndA,
		const VectorType& StartB,
		const VectorType& EndB)
	{
		const VectorType VectorA = EndA - StartA;
		const VectorType VectorB = EndB - StartB;

		const ScalarType S =
			(VectorA.X * (StartA.Y - StartB.Y) - VectorA.Y * (StartA.X - StartB.X)) /
			(VectorA.X * VectorB.Y - VectorA.Y * VectorB.X);

		const ScalarType T =
			(VectorB.X * (StartB.Y - StartA.Y) - VectorB.Y * (StartB.X - StartA.X)) /
			(VectorB.X * VectorA.Y - VectorB.Y * VectorA.X);

		return
			0 <= S && S <= 1 &&
			0 <= T && T <= 1;
	}

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE bool RayTriangleIntersection(
		const VectorType& RayOrigin,
		const VectorType& RayDirection,
		const VectorType& VertexA,
		const VectorType& VertexB,
		const VectorType& VertexC,
		const bool bAllowNegativeTime,
		ScalarType& OutTime,
		VectorType* OutBarycentrics = nullptr)
	{
		checkStatic(std::is_same_v<ScalarType, typename VectorType::FReal>);

		const VectorType Diff = RayOrigin - VertexA;
		const VectorType Edge1 = VertexB - VertexA;
		const VectorType Edge2 = VertexC - VertexA;
		const VectorType Normal = VectorType::CrossProduct(Edge1, Edge2);

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

		ScalarType Dot = RayDirection.Dot(Normal);
		ScalarType Sign;
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

		const ScalarType DotTimesB1 = Sign * RayDirection.Dot(Diff.Cross(Edge2));
		if (DotTimesB1 < 0)
		{
			// b1 < 0, no intersection
			return false;
		}

		const ScalarType DotTimesB2 = Sign * RayDirection.Dot(Edge1.Cross(Diff));
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
		const ScalarType DotTimesT = -Sign * Diff.Dot(Normal);
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

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename VectorType>
	FORCEINLINE VectorType GetTriangleCrossProduct(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C)
	{
		return VectorType::CrossProduct(C - A, B - A);
	}
	template<typename VectorType>
	FORCEINLINE VectorType GetTriangleNormal(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C)
	{
		return GetTriangleCrossProduct(A, B, C).GetSafeNormal();
	}
	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType GetTriangleArea(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C)
	{
		return VectorType::CrossProduct(C - A, B - A).Size() / ScalarType(2);
	}
	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE bool IsTriangleValid(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C,
		const ScalarType Tolerance = KINDA_SMALL_NUMBER)
	{
		return GetTriangleArea(A, B, C) > Tolerance;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FQuat MakeQuaternionFromEuler(double Pitch, double Yaw, double Roll);
	VOXELCORE_API FQuat MakeQuaternionFromBasis(const FVector& X, const FVector& Y, const FVector& Z);
	VOXELCORE_API FQuat MakeQuaternionFromZ(const FVector& Z);
}