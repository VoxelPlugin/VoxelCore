// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "GeomTools.h"

bool FVoxelUtilities::IsPolygonSelfIntersecting(const TConstVoxelArrayView<FVector2D> Polygon)
{
	VOXEL_FUNCTION_COUNTER();

	for (int32 IndexX = 0; IndexX < Polygon.Num(); IndexX++)
	{
		const int32 IndexY = IndexX < Polygon.Num() - 1 ? IndexX + 1 : 0;

		for (int32 IndexU = 0; IndexU < Polygon.Num(); IndexU++)
		{
			const int32 IndexV = IndexU < Polygon.Num() - 1 ? IndexU + 1 : 0;

			if (IndexX == IndexU ||
				IndexX == IndexV ||
				IndexY == IndexU)
			{
				continue;
			}
			checkVoxelSlow(IndexY != IndexV);

			if (AreSegmentsIntersecting(
				Polygon[IndexX],
				Polygon[IndexY],
				Polygon[IndexU],
				Polygon[IndexV]))
			{
				return true;
			}
		}
	}

	return false;
}

bool FVoxelUtilities::IsPolygonWindingCCW(const TConstVoxelArrayView<FVector2D> Polygon)
{
	// https://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order

	checkVoxelSlow(Polygon.Num() >= 3);

	double Sum = 0.f;

	for (int32 Index = 0; Index < Polygon.Num() - 1; Index++)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[Index + 1];
		Sum += (B.X - A.X) * (B.Y + A.Y);
	}

	{
		const FVector2D& A = Polygon.Last();
		const FVector2D& B = Polygon[0];
		Sum += (B.X - A.X) * (B.Y + A.Y);
	}

	return Sum < 0.f;
}

bool FVoxelUtilities::IsPolygonConvex(const TConstVoxelArrayView<FVector2D> Polygon)
{
	checkVoxelSlow(Polygon.Num() >= 3);

	bool bSign = false;
	bool bHasSign = false;

	for (int32 Index = 0; Index < Polygon.Num(); ++Index)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[Index == Polygon.Num() - 1 ? 0 : Index + 1];
		const FVector2D& C = Polygon[Index >= Polygon.Num() - 2 ? Index - (Polygon.Num() - 2) : Index + 2];

		checkVoxelSlow(&B == &Polygon[(Index + 1) % Polygon.Num()]);
		checkVoxelSlow(&C == &Polygon[(Index + 2) % Polygon.Num()]);

		const double CrossProduct = FVector2D::CrossProduct(B - A, C - B);
		if (CrossProduct == 0)
		{
			// Co-linear
			continue;
		}

		const bool bNewSign = CrossProduct > 0;

		if (!bHasSign)
		{
			bSign = bNewSign;
			bHasSign = true;
			continue;
		}

		if (bSign != bNewSign)
		{
			return false;
		}
	}

#if VOXEL_DEBUG
	check(bSign == IsPolygonWindingCCW(Polygon));
#endif

	ensure(bHasSign);
	return true;
}

bool FVoxelUtilities::IsInConvexPolygon(
	const FVector2D& Point,
	const TConstVoxelArrayView<FVector2D> Polygon)
{
	checkVoxelSlow(IsPolygonWindingCCW(Polygon));
	checkVoxelSlow(IsPolygonConvex(Polygon));
	checkVoxelSlow(Polygon.Num() >= 3);

	for (int32 Index = 0; Index < Polygon.Num() - 1; Index++)
	{
		const FVector2D AB = Polygon[Index] - Point;
		const FVector2D AC = Polygon[Index + 1] - Point;
		if (FVector2D::CrossProduct(AB, AC) < 0)
		{
			return false;
		}
	}

	const FVector2D AB = Polygon.Last() - Point;
	const FVector2D AC = Polygon[0] - Point;
	if (FVector2D::CrossProduct(AB, AC) < 0)
	{
		return false;
	}

	return true;
}

bool FVoxelUtilities::SegmentIntersectsPolygon(
	const FVector2D& A,
	const FVector2D& B,
	const TConstVoxelArrayView<FVector2D> Polygon)
{
	checkVoxelSlow(Polygon.Num() >= 3);

	for (int32 Index = 0; Index < Polygon.Num() - 1; Index++)
	{
		if (AreSegmentsIntersecting(
			A,
			B,
			Polygon[Index],
			Polygon[Index + 1]))
		{
			return true;
		}
	}

	return AreSegmentsIntersecting(
		A,
		B,
		Polygon.Last(),
		Polygon[0]);
}

TVoxelArray<TVoxelArray<FVector2D>> FVoxelUtilities::GenerateConvexPolygons(const TConstVoxelArrayView<FVector2D> Polygon)
{
	VOXEL_FUNCTION_COUNTER();
	ensureVoxelSlowNoSideEffects(!IsPolygonSelfIntersecting(Polygon));

	if (!IsPolygonWindingCCW(Polygon))
	{
		TVoxelArray<FVector2D> NewPolygon;
		SetNumFast(NewPolygon, Polygon.Num());

		for (int32 Index = 0; Index < Polygon.Num(); Index++)
		{
			NewPolygon.Last(Index) = Polygon[Index];
		}

		return GenerateConvexPolygonsFromTriangles(TriangulatePolygon(NewPolygon));
	}

	return GenerateConvexPolygonsFromTriangles(TriangulatePolygon(Polygon));
}

TVoxelArray<FVector2D> FVoxelUtilities::TriangulatePolygon(const TConstVoxelArrayView<FVector2D> Polygon)
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(IsPolygonWindingCCW(Polygon));
	checkVoxelSlow(Polygon.Num() >= 3);

	TVoxelArray<FVector2D> Vertices = TVoxelArray<FVector2D>(Polygon);

	TVoxelArray<FVector2D> OutTriangles;
	OutTriangles.Reserve(3 * Vertices.Num());

	while (Vertices.Num() >= 3)
	{
		const bool bFoundEar = INLINE_LAMBDA
		{
			for (int32 IndexB = 0; IndexB < Vertices.Num(); IndexB++)
			{
				const int32 IndexA = IndexB == 0 ? Vertices.Num() - 1 : IndexB - 1;
				const int32 IndexC = IndexB == Vertices.Num() - 1 ? 0 : IndexB + 1;

				const FVector2D VertexA = Vertices[IndexA];
				const FVector2D VertexB = Vertices[IndexB];
				const FVector2D VertexC = Vertices[IndexC];

				// Check that A-B-C is convex
				if (FVector2D::CrossProduct(VertexB - VertexA, VertexC - VertexA) < 0)
				{
					continue;
				}

				const bool bCanAdd = INLINE_LAMBDA
				{
					for (const FVector2D& Vertex : Vertices)
					{
						if (Vertex == VertexA ||
							Vertex == VertexB ||
							Vertex == VertexC)
						{
							continue;
						}

						// If a point is not in the triangle, it may be on the new edge we're adding, which isn't allowed as
						// it will create a partition in the polygon
						if (IsPointInTriangle(Vertex, VertexA, VertexB, VertexC) ||
							IsPointOnSegment(Vertex, VertexC, VertexA))
						{
							return false;
						}
					}

					return true;
				};

				if (!bCanAdd)
				{
					continue;
				}

				OutTriangles.Add_EnsureNoGrow(VertexA);
				OutTriangles.Add_EnsureNoGrow(VertexB);
				OutTriangles.Add_EnsureNoGrow(VertexC);

				Vertices.RemoveAt(IndexB);

				return true;
			}

			return false;
		};

		if (!ensure(bFoundEar))
		{
			return {};
		}
	}

#if VOXEL_DEBUG
	{
		TArray<FVector2D> Triangles;
		FGeomTools2D::TriangulatePoly(Triangles, TArray<FVector2D>(Polygon), true);

		ensure(Triangles == TArray<FVector2D>(OutTriangles));
	}
#endif

	ensure(Vertices.Num() == 2);
	return OutTriangles;
}

TVoxelArray<TVoxelArray<FVector2D>> FVoxelUtilities::GenerateConvexPolygonsFromTriangles(const TConstVoxelArrayView<FVector2D> Triangles)
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(Triangles.Num() % 3 == 0);

	TVoxelArray<TVoxelArray<FVector2D>> OutPolygons;

	TVoxelArray<int32> TrianglesToAdd;
	{
		TrianglesToAdd.Reserve(Triangles.Num() / 3);

		for (int32 Index = 0; Index < Triangles.Num() / 3; Index++)
		{
			TrianglesToAdd.Add_EnsureNoGrow(Index);
		}
	}

	TVoxelArray<FVector2D> Polygon;
	Polygon.Reserve(Triangles.Num());

	while (TrianglesToAdd.Num() > 0)
	{
		checkVoxelSlow(Polygon.Num() == 0);

		{
			const int32 TriangleIndex = TrianglesToAdd.Pop();

			Polygon.Add(Triangles[3 * TriangleIndex + 0]);
			Polygon.Add(Triangles[3 * TriangleIndex + 1]);
			Polygon.Add(Triangles[3 * TriangleIndex + 2]);
		}

		// Find triangles that can be merged into the polygon.

		while (INLINE_LAMBDA
		{
			for (int32 TrianglesToAddIndex = 0; TrianglesToAddIndex < TrianglesToAdd.Num(); TrianglesToAddIndex++)
			{
				const int32 TriangleIndex = TrianglesToAdd[TrianglesToAddIndex];

				const FVector2D VertexA = Triangles[3 * TriangleIndex + 0];
				const FVector2D VertexB = Triangles[3 * TriangleIndex + 1];
				const FVector2D VertexC = Triangles[3 * TriangleIndex + 2];

				for (int32 Index0 = 0; Index0 < Polygon.Num(); Index0++)
				{
					const int32 Index1 = Index0 == Polygon.Num() - 1 ? 0 : Index0 + 1;

					const FVector2D Vertex0 = Polygon[Index0];
					const FVector2D Vertex1 = Polygon[Index1];

					const auto TryAdd = [&](
						const FVector2D& VertexU,
						const FVector2D& VertexV,
						const FVector2D& VertexW)
					{
						if (!Vertex0.Equals(VertexV) ||
							!Vertex1.Equals(VertexU))
						{
							return false;
						}

						if (FVector2D::CrossProduct(VertexW - Vertex0, Vertex1 - VertexW) < 0)
						{
							// Clock-wise triangle, adding this would make the polygon non-convex
#if VOXEL_DEBUG
							TVoxelArray<FVector2D> NewPolygon = Polygon;
							NewPolygon.Insert(VertexW, Index1);
							check(!FVoxelUtilities::IsPolygonConvex(NewPolygon));
#endif
							return false;
						}

						Polygon.Insert(VertexW, Index1);
						checkVoxelSlow(FVoxelUtilities::IsPolygonConvex(Polygon));

						return true;
					};

					if (TryAdd(VertexA, VertexB, VertexC) ||
						TryAdd(VertexB, VertexC, VertexA) ||
						TryAdd(VertexC, VertexA, VertexB))
					{
						TrianglesToAdd.RemoveAtSwap(TrianglesToAddIndex);
						return true;
					}
				}
			}

			return false;
		})
		{
			checkVoxelSlow(FVoxelUtilities::IsPolygonConvex(Polygon));
			checkVoxelSlow(FVoxelUtilities::IsPolygonWindingCCW(Polygon));
		}

		checkVoxelSlow(FVoxelUtilities::IsPolygonConvex(Polygon));
		checkVoxelSlow(FVoxelUtilities::IsPolygonWindingCCW(Polygon));

		OutPolygons.Add(Polygon);
		Polygon.Reset();
	}

#if VOXEL_DEBUG
	TArray<TArray<FVector2D>> NewConvexPolygons;
	FGeomTools2D::GenerateConvexPolygonsFromTriangles(NewConvexPolygons, TArray<FVector2D>(Triangles));

	check(NewConvexPolygons == TArray<TArray<FVector2D>>(OutPolygons));
#endif

	return OutPolygons;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FQuat FVoxelUtilities::MakeQuaternionFromEuler(const double Pitch, const double Yaw, const double Roll)
{
	double SinPitch;
	double CosPitch;
	FMath::SinCos(&SinPitch, &CosPitch, FMath::Fmod(Pitch, 360.0f) * PI / 360.f);

	double SinYaw;
	double CosYaw;
	FMath::SinCos(&SinYaw, &CosYaw, FMath::Fmod(Yaw, 360.0f) * PI / 360.f);

	double SinRoll;
	double CosRoll;
	FMath::SinCos(&SinRoll, &CosRoll, FMath::Fmod(Roll, 360.0f) * PI / 360.f);

	const FQuat Quat = FQuat(
		CosRoll * SinPitch * SinYaw - SinRoll * CosPitch * CosYaw,
		-CosRoll * SinPitch * CosYaw - SinRoll * CosPitch * SinYaw,
		CosRoll * CosPitch * SinYaw - SinRoll * SinPitch * CosYaw,
		CosRoll * CosPitch * CosYaw + SinRoll * SinPitch * SinYaw);

	checkVoxelSlow(Quat.Equals(FRotator(Pitch, Yaw, Roll).Quaternion()));
	return Quat;
}

FQuat FVoxelUtilities::MakeQuaternionFromBasis(const FVector& X, const FVector& Y, const FVector& Z)
{
	FQuat Quat;

	if (X.X + Y.Y + Z.Z > 0.0f)
	{
		const double InvS = FMath::InvSqrt(X.X + Y.Y + Z.Z + 1.0f);
		const double S = 0.5f * InvS;

		Quat.X = (Y.Z - Z.Y) * S;
		Quat.Y = (Z.X - X.Z) * S;
		Quat.Z = (X.Y - Y.X) * S;
		Quat.W = 0.5f * (1.f / InvS);
	}
	else if (X.X > Y.Y && X.X > Z.Z)
	{
		const double InvS = FMath::InvSqrt(X.X - Y.Y - Z.Z + 1.0f);
		const double S = 0.5f * InvS;

		Quat.X = 0.5f * (1.f / InvS);
		Quat.Y = (X.Y + Y.X) * S;
		Quat.Z = (X.Z + Z.X) * S;
		Quat.W = (Y.Z - Z.Y) * S;
	}
	else if (Y.Y > X.X && Y.Y > Z.Z)
	{
		const double InvS = FMath::InvSqrt(Y.Y - Z.Z - X.X + 1.0f);
		const double S = 0.5f * InvS;

		Quat.Y = 0.5f * (1.f / InvS);
		Quat.Z = (Y.Z + Z.Y) * S;
		Quat.X = (Y.X + X.Y) * S;
		Quat.W = (Z.X - X.Z) * S;
	}
	else
	{
		checkVoxelSlow(Z.Z >= X.X && Z.Z >= Y.Y);

		const double InvS = FMath::InvSqrt(Z.Z - X.X - Y.Y + 1.0f);
		const double S = 0.5f * InvS;

		Quat.Z = 0.5f * (1.f / InvS);
		Quat.X = (Z.X + X.Z) * S;
		Quat.Y = (Z.Y + Y.Z) * S;
		Quat.W = (X.Y - Y.X) * S;
	}

	checkVoxelSlow(Quat.Equals(FMatrix(X, Y, Z, FVector::Zero()).ToQuat()));
	return Quat;
}

FQuat FVoxelUtilities::MakeQuaternionFromZ(const FVector& Z)
{
	const FVector NewZ = Z.GetSafeNormal();

	// Try to use up if possible
	const FVector UpVector = FMath::Abs(NewZ.Z) < (1.f - KINDA_SMALL_NUMBER) ? FVector(0, 0, 1.f) : FVector(1.f, 0, 0);

	const FVector NewX = FVector::CrossProduct(UpVector, NewZ).GetSafeNormal();
	const FVector NewY = FVector::CrossProduct(NewZ, NewX);

	const FQuat Quat = MakeQuaternionFromBasis(NewX, NewY, NewZ);
	checkVoxelSlow(Quat.Equals(FRotationMatrix::MakeFromZ(Z).ToQuat()));
	return Quat;
}