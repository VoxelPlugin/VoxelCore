// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

namespace FVoxelUtilities
{
	template<typename T>
	using TVector2 = UE::Math::TVector2<T>;

	template<typename T>
	using TVector = UE::Math::TVector<T>;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// O(n2)
	VOXELCORE_API bool IsPolygonSelfIntersecting(TConstVoxelArrayView<FVector2D> Polygon);
	VOXELCORE_API bool IsPolygonWindingCCW(TConstVoxelArrayView<FVector2D> Polygon);
	VOXELCORE_API bool IsPolygonConvex(TConstVoxelArrayView<FVector2D> Polygon);

	VOXELCORE_API bool IsInConvexPolygon(
		const FVector2D& Point,
		TConstVoxelArrayView<FVector2D> Polygon);

	// Will return false if the segment is fully contained within the polygon
	VOXELCORE_API bool SegmentIntersectsPolygon(
		const FVector2D& A,
		const FVector2D& B,
		TConstVoxelArrayView<FVector2D> Polygon);

	VOXELCORE_API TVoxelArray<TVoxelArray<FVector2D>> GenerateConvexPolygons(TConstVoxelArrayView<FVector2D> Polygon);

	VOXELCORE_API TVoxelArray<FVector2D> TriangulatePolygon(TConstVoxelArrayView<FVector2D> Polygon);
	VOXELCORE_API TVoxelArray<TVoxelArray<FVector2D>> GenerateConvexPolygonsFromTriangles(TConstVoxelArrayView<FVector2D> Triangles);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename Real>
	FORCEINLINE bool RayTriangleIntersection(
		const TVector<Real>& RayOrigin,
		const TVector<Real>& RayDirection,
		const TVector<Real>& VertexA,
		const TVector<Real>& VertexB,
		const TVector<Real>& VertexC,
		const bool bAllowNegativeTime,
		Real& OutTime,
		TVector<Real>* OutBarycentrics = nullptr)
	{
		const TVector<Real> Diff = RayOrigin - VertexA;
		const TVector<Real> Edge1 = VertexB - VertexA;
		const TVector<Real> Edge2 = VertexC - VertexA;
		const TVector<Real> Normal = TVector<Real>::CrossProduct(Edge1, Edge2);

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

		Real Dot = RayDirection.Dot(Normal);
		Real Sign;
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

		const Real DotTimesB1 = Sign * RayDirection.Dot(Diff.Cross(Edge2));
		if (DotTimesB1 < 0)
		{
			// b1 < 0, no intersection
			return false;
		}

		const Real DotTimesB2 = Sign * RayDirection.Dot(Edge1.Cross(Diff));
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
		const Real DotTimesT = -Sign * Diff.Dot(Normal);
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

	template<typename Real>
	FORCEINLINE TVector<Real> GetTriangleNormal(
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C)
	{
		return TVector<Real>::CrossProduct(C - A, B - A).GetSafeNormal();
	}
	template<typename Real>
	FORCEINLINE Real GetTriangleArea(
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C)
	{
		return TVector<Real>::CrossProduct(C - A, B - A).Size() / Real(2);
	}
	template<typename Real>
	FORCEINLINE Real GetTriangleAreaSquared(
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C)
	{
		return TVector<Real>::CrossProduct(C - A, B - A).SizeSquared() / Real(4);
	}
	template<typename Real>
	FORCEINLINE bool IsTriangleValid(
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C,
		const Real Tolerance = KINDA_SMALL_NUMBER)
	{
		checkVoxelSlow((GetTriangleAreaSquared(A, B, C) > FMath::Square(Tolerance)) == (GetTriangleArea(A, B, C) > Tolerance));
		return GetTriangleAreaSquared(A, B, C) > FMath::Square(Tolerance);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename Real>
	FORCEINLINE void GetTriangleBarycentrics(
		const TVector<Real>& Point,
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C,
		TVector<Real>& OutBarycentrics)
	{
		const TVector<Real> CA = A - C;
		const TVector<Real> CB = B - C;
		const TVector<Real> CP = Point - C;

		const Real SizeCA = CA.SizeSquared();
		const Real SizeCB = CB.SizeSquared();

		const Real a = TVector<Real>::DotProduct(CA, CP);
		const Real b = TVector<Real>::DotProduct(CB, CP);
		const Real d = TVector<Real>::DotProduct(CA, CB);

		const Real Determinant = SizeCA * SizeCB - d * d;
		ensureVoxelSlow(Determinant > 0.f);

		OutBarycentrics.X = (SizeCB * a - d * b) / Determinant;
		OutBarycentrics.Y = (SizeCA * b - d * a) / Determinant;
		OutBarycentrics.Z = 1 - OutBarycentrics.X - OutBarycentrics.Y;

		ensureVoxelSlow(
			OutBarycentrics.X > 0 ||
			OutBarycentrics.Y > 0 ||
			OutBarycentrics.Z > 0);
	}

	template<typename Real>
	FORCEINLINE Real ProjectPointOnLine(
		const TVector<Real>& Point,
		const TVector<Real>& A,
		const TVector<Real>& B)
	{
		const TVector<Real> AB = B - A;
		const TVector<Real> AP = Point - A;

		// return TVector<Real>::DotProduct(AP, AB.GetSafeNormal()) / AB.Size();
		// return TVector<Real>::DotProduct(AP, AB / AB.Size()) / AB.Size();
		// return TVector<Real>::DotProduct(AP, AB) / AB.Size() / AB.Size();
		return TVector<Real>::DotProduct(AP, AB) / AB.SizeSquared();
	}

	template<typename Real>
	FORCEINLINE Real PointSegmentDistanceSquared(
		const TVector<Real>& Point,
		const TVector<Real>& A,
		const TVector<Real>& B,
		Real& OutAlphaB)
	{
		Real Alpha = ProjectPointOnLine(Point, A, B);
		// Clamp to AB
		Alpha = FMath::Clamp<Real>(Alpha, 0, 1);

		OutAlphaB = Alpha;

		const TVector<Real> ProjectedPoint = FMath::Lerp(A, B, Alpha);
		return TVector<Real>::DistSquared(Point, ProjectedPoint);
	}

	template<typename Real>
	FORCEINLINE Real PointTriangleDistanceSquared(
		const TVector<Real>& Point,
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C,
		TVector<Real>& OutBarycentrics)
	{
		FVoxelUtilities::GetTriangleBarycentrics(
			Point,
			A,
			B,
			C,
			OutBarycentrics);

		if (OutBarycentrics.X >= 0 &&
			OutBarycentrics.Y >= 0 &&
			OutBarycentrics.Z >= 0)
		{
			// We're inside the triangle
			return TVector<Real>::DistSquared(
				Point,
				OutBarycentrics.X * A +
				OutBarycentrics.Y * B +
				OutBarycentrics.Z * C);
		}

		if (OutBarycentrics.X > 0)
		{
			// Rules out BC

			Real AlphaB_AB;
			const Real DistanceAB = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, B, AlphaB_AB);

			Real AlphaC_AC;
			const Real DistanceAC = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, C, AlphaC_AC);

			if (DistanceAB < DistanceAC)
			{
				OutBarycentrics.X = 1 - AlphaB_AB;
				OutBarycentrics.Y = AlphaB_AB;
				OutBarycentrics.Z = 0;
				return DistanceAB;
			}
			else
			{
				OutBarycentrics.X = 1 - AlphaC_AC;
				OutBarycentrics.Y = 0;
				OutBarycentrics.Z = AlphaC_AC;
				return DistanceAC;
			}
		}

		if (OutBarycentrics.Y > 0)
		{
			// Rules out AC

			Real AlphaB_AB;
			const Real DistanceAB = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, B, AlphaB_AB);

			Real AlphaC_BC;
			const Real DistanceBC = FVoxelUtilities::PointSegmentDistanceSquared(Point, B, C, AlphaC_BC);

			if (DistanceAB < DistanceBC)
			{
				OutBarycentrics.X = 1 - AlphaB_AB;
				OutBarycentrics.Y = AlphaB_AB;
				OutBarycentrics.Z = 0;
				return DistanceAB;
			}
			else
			{
				OutBarycentrics.X = 0;
				OutBarycentrics.Y = 1 - AlphaC_BC;
				OutBarycentrics.Z = AlphaC_BC;
				return DistanceBC;
			}
		}

		ensureVoxelSlow(OutBarycentrics.Z > 0);

		// Rules out AB

		Real AlphaC_BC;
		const Real DistanceBC = FVoxelUtilities::PointSegmentDistanceSquared(Point, B, C, AlphaC_BC);

		Real AlphaC_AC;
		const Real DistanceAC = FVoxelUtilities::PointSegmentDistanceSquared(Point, A, C, AlphaC_AC);

		if (DistanceBC < DistanceAC)
		{
			OutBarycentrics.X = 0;
			OutBarycentrics.Y = AlphaC_BC;
			OutBarycentrics.Z = 1 - AlphaC_BC;
			return DistanceBC;
		}
		else
		{
			OutBarycentrics.X = AlphaC_AC;
			OutBarycentrics.Y = 0;
			OutBarycentrics.Z = 1 - AlphaC_AC;
			return DistanceAC;
		}
	}

	template<typename Real>
	FORCEINLINE Real PointTriangleDistanceSquared(
		const TVector<Real>& Point,
		const TVector<Real>& A,
		const TVector<Real>& B,
		const TVector<Real>& C)
	{
		TVector<Real> Barycentrics;
		return FVoxelUtilities::PointTriangleDistanceSquared(
			Point,
			A,
			B,
			C,
			Barycentrics);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename Real>
	FORCEINLINE bool AreSegmentsIntersecting(
		const TVector2<Real>& StartA,
		const TVector2<Real>& EndA,
		const TVector2<Real>& StartB,
		const TVector2<Real>& EndB)
	{
		const TVector2<Real> VectorA = EndA - StartA;
		const TVector2<Real> VectorB = EndB - StartB;

		const Real S =
			(VectorA.X * (StartA.Y - StartB.Y) - VectorA.Y * (StartA.X - StartB.X)) /
			(VectorA.X * VectorB.Y - VectorA.Y * VectorB.X);

		const Real T =
			(VectorB.X * (StartB.Y - StartA.Y) - VectorB.Y * (StartB.X - StartA.X)) /
			(VectorB.X * VectorA.Y - VectorB.Y * VectorA.X);

		return
			0 <= S && S <= 1 &&
			0 <= T && T <= 1;
	}

	// False if point is on the edge
	template<typename Real>
	FORCEINLINE bool IsPointInTriangle(
		const TVector2<Real>& P,
		const TVector2<Real>& A,
		const TVector2<Real>& B,
		const TVector2<Real>& C)
	{
		// Compute the sign for each edge, ie whether P is left or right of the edge
		// If the sign is the same for the 3 edges, we are inside

		const Real AB = TVector2<Real>::CrossProduct(P - A, B - A);
		const Real BC = TVector2<Real>::CrossProduct(P - B, C - B);
		const Real CA = TVector2<Real>::CrossProduct(P - C, A - C);

		// We can't be on all edges at once
		checkVoxelSlow(AB != 0 || BC != 0 || CA != 0 || (A == B && B == C));

		// A bit convoluted as we want to return false if there's a zero
		const bool bHasNegative = AB < 0 || BC < 0 || CA < 0;
		const bool bHasPositive = AB > 0 || BC > 0 || CA > 0;

		const bool bResult = !(bHasNegative && bHasPositive);

		// If we are on any edge, we are not inside
		checkVoxelSlow(!bResult || (AB != 0 && BC != 0 && CA != 0));

#if VOXEL_DEBUG
		TVector<Real> Barycentrics;
		GetTriangleBarycentrics(
			TVector<Real>(P, 0),
			TVector<Real>(A, 0),
			TVector<Real>(B, 0),
			TVector<Real>(C, 0),
			Barycentrics);

		check(bResult == (
			Barycentrics.X >= 0 &&
			Barycentrics.Y >= 0 &&
			Barycentrics.Z >= 0));
#endif

		return bResult;
	}

	template<typename Real>
	FORCEINLINE bool IsPointOnSegment(
		const TVector2<Real>& P,
		const TVector2<Real>& A,
		const TVector2<Real>& B,
		const Real Tolerance = KINDA_SMALL_NUMBER)
	{
		if ((P.X < A.X && P.X < B.X) ||
			(P.Y < A.Y && P.Y < B.Y) ||
			(P.X > A.X && P.X > B.X) ||
			(P.Y > A.Y && P.Y > B.Y))
		{
			// Outside the box made by (A, B)
			// Don't use tolerance here
			return false;
		}

		const TVector2<Real> BA = B - A;
		const TVector2<Real> PA = P - A;

		const Real SizeSquaredBA = BA.SizeSquared();
		const Real ParallelogramArea = TVector2<Real>::CrossProduct(BA, PA);

		if (FMath::Abs(ParallelogramArea) > Tolerance * SizeSquaredBA)
		{
			return false;
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FQuat MakeQuaternionFromEuler(double Pitch, double Yaw, double Roll);
	VOXELCORE_API FQuat MakeQuaternionFromBasis(const FVector& X, const FVector& Y, const FVector& Z);
	VOXELCORE_API FQuat MakeQuaternionFromZ(const FVector& Z);
}