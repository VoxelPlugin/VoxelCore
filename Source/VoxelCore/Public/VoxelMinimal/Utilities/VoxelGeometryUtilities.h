// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE bool AreSegmentsIntersecting(
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
	FORCEINLINE ScalarType GetTriangleAreaSquared(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C)
	{
		return VectorType::CrossProduct(C - A, B - A).SizeSquared() / ScalarType(4);
	}
	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE bool IsTriangleValid(
		const VectorType& A,
		const VectorType& B,
		const VectorType& C,
		const ScalarType Tolerance = KINDA_SMALL_NUMBER)
	{
		checkVoxelSlow((GetTriangleAreaSquared(A, B, C) > FMath::Square(Tolerance)) == (GetTriangleArea(A, B, C) > Tolerance));
		return GetTriangleAreaSquared(A, B, C) > FMath::Square(Tolerance);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE void GetTriangleBarycentrics(
		const VectorType& Point,
		const VectorType& A,
		const VectorType& B,
		const VectorType& C,
		ScalarType& OutAlphaA,
		ScalarType& OutAlphaB,
		ScalarType& OutAlphaC)
	{
		const VectorType CA = A - C;
		const VectorType CB = B - C;
		const VectorType CP = Point - C;

		const ScalarType SizeCA = CA.SizeSquared();
		const ScalarType SizeCB = CB.SizeSquared();

		const ScalarType a = VectorType::DotProduct(CA, CP);
		const ScalarType b = VectorType::DotProduct(CB, CP);
		const ScalarType d = VectorType::DotProduct(CA, CB);

		const ScalarType Determinant = SizeCA * SizeCB - d * d;
		checkVoxelSlow(Determinant > 0.f);

		OutAlphaA = (SizeCB * a - d * b) / Determinant;
		OutAlphaB = (SizeCA * b - d * a) / Determinant;
		OutAlphaC = 1 - OutAlphaA - OutAlphaB;

		ensureVoxelSlow(OutAlphaA > 0 || OutAlphaB > 0 || OutAlphaC > 0);
	}

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType ProjectPointOnLine(
		const VectorType& Point,
		const VectorType& A,
		const VectorType& B)
	{
		const VectorType AB = B - A;
		const VectorType AP = Point - A;

		// return VectorType::DotProduct(AP, AB.GetSafeNormal()) / AB.Size();
		// return VectorType::DotProduct(AP, AB / AB.Size()) / AB.Size();
		// return VectorType::DotProduct(AP, AB) / AB.Size() / AB.Size();
		return VectorType::DotProduct(AP, AB) / AB.SizeSquared();
	}

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType PointSegmentDistanceSquared(
		const VectorType& Point,
		const VectorType& A,
		const VectorType& B,
		ScalarType& OutAlphaB)
	{
		ScalarType Alpha = ProjectPointOnLine(Point, A, B);
		// Clamp to AB
		Alpha = FMath::Clamp<ScalarType>(Alpha, 0, 1);

		OutAlphaB = Alpha;

		const VectorType ProjectedPoint = FMath::Lerp(A, B, Alpha);
		return VectorType::DistSquared(Point, ProjectedPoint);
	}

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType PointTriangleDistanceSquared(
		const VectorType& Point,
		const VectorType& A,
		const VectorType& B,
		const VectorType& C,
		ScalarType& OutAlphaA,
		ScalarType& OutAlphaB,
		ScalarType& OutAlphaC)
	{
		FVoxelUtilities::GetTriangleBarycentrics(
			Point,
			A,
			B,
			C,
			OutAlphaA,
			OutAlphaB,
			OutAlphaC);

		if (OutAlphaA >= 0 &&
			OutAlphaB >= 0 &&
			OutAlphaC >= 0)
		{
			// We're inside the triangle
			return VectorType::DistSquared(Point, OutAlphaA * A + OutAlphaB * B + OutAlphaC * C);
		}

		if (OutAlphaA > 0)
		{
			// Rules out BC

			ScalarType AlphaB_AB;
			const ScalarType DistanceAB = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, B, AlphaB_AB);

			ScalarType AlphaC_AC;
			const ScalarType DistanceAC = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, C, AlphaC_AC);

			if (DistanceAB < DistanceAC)
			{
				OutAlphaA = 1 - AlphaB_AB;
				OutAlphaB = AlphaB_AB;
				OutAlphaC = 0;
				return DistanceAB;
			}
			else
			{
				OutAlphaA = 1 - AlphaC_AC;
				OutAlphaB = 0;
				OutAlphaC = AlphaC_AC;
				return DistanceAC;
			}
		}

		if (OutAlphaB > 0)
		{
			// Rules out AC

			ScalarType AlphaB_AB;
			const ScalarType DistanceAB = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, B, AlphaB_AB);

			ScalarType AlphaC_BC;
			const ScalarType DistanceBC = FVoxelUtilities::PointSegmentDistanceSquared(Point, B, C, AlphaC_BC);

			if (DistanceAB < DistanceBC)
			{
				OutAlphaA = 1 - AlphaB_AB;
				OutAlphaB = AlphaB_AB;
				OutAlphaC = 0;
				return DistanceAB;
			}
			else
			{
				OutAlphaA = 0;
				OutAlphaB = 1 - AlphaC_BC;
				OutAlphaC = AlphaC_BC;
				return DistanceBC;
			}
		}

		ensureVoxelSlow(OutAlphaC > 0);

		// Rules out AB

		ScalarType AlphaC_BC;
		const ScalarType DistanceBC = FVoxelUtilities::PointSegmentDistanceSquared(Point, B, C, AlphaC_BC);

		ScalarType AlphaC_AC;
		const ScalarType DistanceAC = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, C, AlphaC_AC);

		if (DistanceBC < DistanceAC)
		{
			OutAlphaA = 0;
			OutAlphaB = AlphaC_BC;
			OutAlphaC = 1 - AlphaC_BC;
			return DistanceBC;
		}
		else
		{
			OutAlphaA = AlphaC_AC;
			OutAlphaB = 0;
			OutAlphaC = 1 - AlphaC_AC;
			return DistanceAC;
		}
	}

	template<typename VectorType, typename ScalarType = typename VectorType::FReal>
	FORCEINLINE ScalarType PointTriangleDistanceSquared(
		const VectorType& Point,
		const VectorType& A,
		const VectorType& B,
		const VectorType& C)
	{
		ScalarType AlphaA;
		ScalarType AlphaB;
		ScalarType AlphaC;
		return FVoxelUtilities::PointTriangleDistanceSquared(
			Point,
			A,
			B,
			C,
			AlphaA,
			AlphaB,
			AlphaC);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FQuat MakeQuaternionFromEuler(double Pitch, double Yaw, double Roll);
	VOXELCORE_API FQuat MakeQuaternionFromBasis(const FVector& X, const FVector& Y, const FVector& Z);
	VOXELCORE_API FQuat MakeQuaternionFromZ(const FVector& Z);
}