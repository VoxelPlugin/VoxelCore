// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.isph"

// ReSharper disable CppCStyleCast

export void VoxelDistanceFieldUtilities_JumpFlood_Initialize(
	const uniform int32 Z,
	const uniform int32 SizeX,
	const uniform int32 SizeY,
	const uniform int32 SizeZ,
	const uniform float Distances[],
	uniform float OutClosestX[],
	uniform float OutClosestY[],
	uniform float OutClosestZ[])
{
	const uniform int32 SizeXY = SizeX * SizeY;

	for (uniform int32 Y = 0; Y < SizeY; Y++)
	{
		FOREACH(X, 0, SizeX)
		{
			const varying int32 Index = X + Y * SizeX + Z * SizeXY;
			const varying float Distance = Distances[Index];
			const varying bool IsDistanceNegative = Distance < 0.f;

			varying float BestNeighborDistance = 0.f;
			varying int32 BestNeighborDirection = -1;

#define	CheckNeighbor(NeighborDirection, DX, DY, DZ) \
			if ((DX >= 0 || X + DX >= 0) && \
				(DY >= 0 || Y + DY >= 0) && \
				(DZ >= 0 || Z + DZ >= 0) && \
				(DX <= 0 || X + DX < SizeX) && \
				(DY <= 0 || Y + DY < SizeY) && \
				(DZ <= 0 || Z + DZ < SizeZ)) \
			{ \
				const varying float NeighborDistance = Distances[Index + DX + DY * SizeX + DZ * SizeXY]; \
				/* If IsDistanceNegative: IsBetter = NeighborValue >= BestNeighborValue (we want positive with the highest absolute value) */ \
				/* If !IsDistanceNegative: IsBetter = NeighborValue < BestNeighborValue (we want negative with the highest absolute value) */ \
				/* Note: 0 is defined as positive, hence the >= and < */ \
				const varying bool IsBetter = \
					intbits(NeighborDistance) != NaNf_uint && \
					IsDistanceNegative != (NeighborDistance < BestNeighborDistance); \
				\
				BestNeighborDistance = select(IsBetter, NeighborDistance, BestNeighborDistance); \
				BestNeighborDirection = select(IsBetter, NeighborDirection, BestNeighborDirection); \
			}

			CheckNeighbor(0, -1, 0, 0);
			CheckNeighbor(1, +1, 0, 0);
			CheckNeighbor(2, 0, -1, 0);
			CheckNeighbor(3, 0, +1, 0);
			CheckNeighbor(4, 0, 0, -1);
			CheckNeighbor(5, 0, 0, +1);

#undef CheckNeighbor

			coherent_if (
				intbits(Distance) == NaNf_uint ||
				BestNeighborDirection == -1)
			{
				OutClosestX[Index] = NaNf;
				OutClosestY[Index] = NaNf;
				OutClosestZ[Index] = NaNf;
				continue;
			}

			const varying float Alpha = Distance / (Distance - BestNeighborDistance);

			varying float ClosestX = X;
			varying float ClosestY = Y;
			varying float ClosestZ = Z;

			ClosestX -= select(BestNeighborDirection == 0, Alpha, 0.f);
			ClosestY -= select(BestNeighborDirection == 2, Alpha, 0.f);
			ClosestZ -= select(BestNeighborDirection == 4, Alpha, 0.f);

			ClosestX += select(BestNeighborDirection == 1, Alpha, 0.f);
			ClosestY += select(BestNeighborDirection == 3, Alpha, 0.f);
			ClosestZ += select(BestNeighborDirection == 5, Alpha, 0.f);

			OutClosestX[Index] = ClosestX;
			OutClosestY[Index] = ClosestY;
			OutClosestZ[Index] = ClosestZ;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void VoxelDistanceFieldUtilities_JumpFlood_JumpFlood(
	const uniform int32 Z,
	const uniform int32 SizeX,
	const uniform int32 SizeY,
	const uniform int32 SizeZ,
	const uniform int32 Step,
	const uniform float InClosestX[],
	const uniform float InClosestY[],
	const uniform float InClosestZ[],
	uniform float OutClosestX[],
	uniform float OutClosestY[],
	uniform float OutClosestZ[])
{
	const uniform int32 SizeXY = SizeX * SizeY;

	for (uniform int32 Y = 0; Y < SizeY; Y++)
	{
		FOREACH(X, 0, SizeX)
		{
			const varying int32 BaseIndex = X + SizeX * Y + SizeXY * Z;

			float BestDistance = MAX_flt;
			float ClosestX = NaNf;
			float ClosestY = NaNf;
			float ClosestZ = NaNf;

#define	CheckNeighbor(DX, DY, DZ) \
			if ((DX >= 0 || X + Step * DX >= 0) && \
				(DY >= 0 || Y + Step * DY >= 0) && \
				(DZ >= 0 || Z + Step * DZ >= 0) && \
				(DX <= 0 || X + Step * DX < SizeX) && \
				(DY <= 0 || Y + Step * DY < SizeY) && \
				(DZ <= 0 || Z + Step * DZ < SizeZ)) \
			{ \
				check(0 <= X + Step * DX && X + Step * DX + SizeX); \
				check(0 <= Y + Step * DY && Y + Step * DY + SizeY); \
				check(0 <= Z + Step * DZ && Z + Step * DZ + SizeZ); \
				\
				const varying float NeighborClosestX = InClosestX[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
				\
				if (any(intbits(NeighborClosestX) != NaNf_uint)) \
				{ \
					const varying float NeighborClosestY = InClosestY[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
					const varying float NeighborClosestZ = InClosestZ[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
					\
					const varying float Distance = \
						Square(X - NeighborClosestX) + \
						Square(Y - NeighborClosestY) + \
						Square(Z - NeighborClosestZ); \
					\
					if (intbits(NeighborClosestX) != NaNf_uint && \
						Distance < BestDistance) \
					{ \
						BestDistance = Distance; \
						ClosestX = NeighborClosestX; \
						ClosestY = NeighborClosestY; \
						ClosestZ = NeighborClosestZ; \
					} \
				} \
			}

			// Check center first to try to keep consistent closest points
			CheckNeighbor(+0, +0, +0);

			CheckNeighbor(-1, -1, -1);
			CheckNeighbor(+0, -1, -1);
			CheckNeighbor(+1, -1, -1);
			CheckNeighbor(-1, +0, -1);
			CheckNeighbor(+0, +0, -1);
			CheckNeighbor(+1, +0, -1);
			CheckNeighbor(-1, +1, -1);
			CheckNeighbor(+0, +1, -1);
			CheckNeighbor(+1, +1, -1);

			CheckNeighbor(-1, -1, +0);
			CheckNeighbor(+0, -1, +0);
			CheckNeighbor(+1, -1, +0);
			CheckNeighbor(-1, +0, +0);
			//CheckNeighbor(+0, +0, +0);
			CheckNeighbor(+1, +0, +0);
			CheckNeighbor(-1, +1, +0);
			CheckNeighbor(+0, +1, +0);
			CheckNeighbor(+1, +1, +0);

			CheckNeighbor(-1, -1, +1);
			CheckNeighbor(+0, -1, +1);
			CheckNeighbor(+1, -1, +1);
			CheckNeighbor(-1, +0, +1);
			CheckNeighbor(+0, +0, +1);
			CheckNeighbor(+1, +0, +1);
			CheckNeighbor(-1, +1, +1);
			CheckNeighbor(+0, +1, +1);
			CheckNeighbor(+1, +1, +1);

#undef CheckNeighbor

			OutClosestX[BaseIndex] = ClosestX;
			OutClosestY[BaseIndex] = ClosestY;
			OutClosestZ[BaseIndex] = ClosestZ;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void VoxelDistanceFieldUtilities_JumpFlood_ComputeDistances(
	const uniform int32 Z,
	const uniform float VoxelSize,
	const uniform int32 SizeX,
	const uniform int32 SizeY,
	const uniform int32 SizeZ,
	const uniform float InClosestX[],
	const uniform float InClosestY[],
	const uniform float InClosestZ[],
	uniform float OutDistances[])
{
	const uniform int32 SizeXY = SizeX * SizeY;

	for (uniform int32 Y = 0; Y < SizeY; Y++)
	{
		FOREACH(X, 0, SizeX)
		{
			const varying int32 Index = X + SizeX * Y + SizeXY * Z;
			const varying float ClosestX = InClosestX[Index];
			const varying float ClosestY = InClosestY[Index];
			const varying float ClosestZ = InClosestZ[Index];

			if (intbits(ClosestX) == NaNf_uint)
			{
				OutDistances[Index] = NaNf;
				continue;
			}
			check(intbits(ClosestY) != NaNf_uint);
			check(intbits(ClosestZ) != NaNf_uint);

			const varying float SquaredDistance =
				Square(ClosestX - X) +
				Square(ClosestY - Y) +
				Square(ClosestZ - Z);

			OutDistances[Index] =
				sqrt(SquaredDistance) *
				(OutDistances[Index] < 0.f ? -1.f : 1.f) *
				VoxelSize;
		}
	}
}