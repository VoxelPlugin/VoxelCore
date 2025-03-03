// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.isph"

// ReSharper disable CppCStyleCast

export void VoxelDistanceFieldUtilities_JumpFlood_Initialize(
	const uniform int32 StartX,
	const uniform int32 StartY,
	const uniform int32 StartZ,
	const uniform int32 EndX,
	const uniform int32 EndY,
	const uniform int32 EndZ,
	const uniform int32 SizeX,
	const uniform int32 SizeY,
	const uniform int32 SizeZ,
	const uniform float Distances[],
	uniform float OutClosestX[],
	uniform float OutClosestY[],
	uniform float OutClosestZ[])
{
	const uniform int32 SizeXY = SizeX * SizeY;

	for (uniform int32 Z = StartZ; Z < EndZ; Z++)
	{
		for (uniform int32 Y = StartY; Y < EndY; Y++)
		{
			FOREACH(X, StartX, EndX)
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
					/* If IsDistanceNegative: IsBetter = NeighborValue > BestNeighborValue (we want positive with the highest absolute value) */ \
					/* If !IsDistanceNegative: IsBetter = NeighborValue < BestNeighborValue (we want negative with the highest absolute value) */ \
					const varying bool IsBetter = \
						intbits(NeighborDistance) != 0xFFFFFFFF && \
						IsDistanceNegative == (NeighborDistance > BestNeighborDistance); \
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
					intbits(Distance) == 0xFFFFFFFF ||
					BestNeighborDirection == -1)
				{
					OutClosestX[Index] = floatbits(0xFFFFFFFF);
					OutClosestY[Index] = floatbits(0xFFFFFFFF);
					OutClosestZ[Index] = floatbits(0xFFFFFFFF);
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
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void VoxelDistanceFieldUtilities_JumpFlood_JumpFlood(
	const uniform int32 StartX,
	const uniform int32 StartY,
	const uniform int32 StartZ,
	const uniform int32 EndX,
	const uniform int32 EndY,
	const uniform int32 EndZ,
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

	for (uniform int32 Z = StartZ; Z < EndZ; Z++)
	{
		const uniform float FloatZ = Z;

#define	CheckNeighbor(DX, DY, DZ, ShouldCheckX, ShouldCheckY, ShouldCheckZ) \
		if ((DX >= 0 || !ShouldCheckX || X + Step * DX >= 0) && \
			(DY >= 0 || !ShouldCheckY || Y + Step * DY >= 0) && \
			(DZ >= 0 || !ShouldCheckZ || Z + Step * DZ >= 0) && \
			(DX <= 0 || !ShouldCheckX || X + Step * DX < SizeX) && \
			(DY <= 0 || !ShouldCheckY || Y + Step * DY < SizeY) && \
			(DZ <= 0 || !ShouldCheckZ || Z + Step * DZ < SizeZ)) \
		{ \
			check(0 <= X + Step * DX && X + Step * DX < SizeX); \
			check(0 <= Y + Step * DY && Y + Step * DY < SizeY); \
			check(0 <= Z + Step * DZ && Z + Step * DZ < SizeZ); \
			\
			const varying float NeighborClosestX = InClosestX[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
			const varying float NeighborClosestY = InClosestY[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
			const varying float NeighborClosestZ = InClosestZ[BaseIndex + Step * (DX + DY * SizeX + DZ * SizeXY)]; \
			\
			if (any(intbits(NeighborClosestX) != 0xFFFFFFFF)) \
			{ \
				const varying float Distance = \
					Square(FloatX - NeighborClosestX) + \
					Square(FloatY - NeighborClosestY) + \
					Square(FloatZ - NeighborClosestZ); \
				\
				if (intbits(NeighborClosestX) != 0xFFFFFFFF && \
					Distance < BestDistance) \
				{ \
					BestDistance = Distance; \
					ClosestX = NeighborClosestX; \
					ClosestY = NeighborClosestY; \
					ClosestZ = NeighborClosestZ; \
				} \
			} \
		}

#define Check(ShouldCheckX, ShouldCheckY, ShouldCheckZ) \
		FOREACH(X, StartX, EndX) \
		{ \
			const varying float FloatX = X; \
			const varying int32 BaseIndex = X + SizeX * Y + SizeXY * Z; \
			\
			float BestDistance = MAX_flt; \
			float ClosestX = floatbits(0xFFFFFFFF); \
			float ClosestY = floatbits(0xFFFFFFFF); \
			float ClosestZ = floatbits(0xFFFFFFFF); \
			\
			/* Check center first to try to keep consistent closest points */ \
			CheckNeighbor(+0, +0, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			\
			CheckNeighbor(-1, -1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, -1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, -1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +0, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, +0, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +0, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, +1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +1, -1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			\
			CheckNeighbor(-1, -1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, -1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, -1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +0, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +0, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, +1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +1, +0, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			\
			CheckNeighbor(-1, -1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, -1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, -1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +0, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, +0, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +0, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(-1, +1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+0, +1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			CheckNeighbor(+1, +1, +1, ShouldCheckX, ShouldCheckY, ShouldCheckZ); \
			\
			OutClosestX[BaseIndex] = ClosestX; \
			OutClosestY[BaseIndex] = ClosestY; \
			OutClosestZ[BaseIndex] = ClosestZ; \
		}

#define CheckX(ShouldCheckY, ShouldCheckZ) \
		if (0 <= StartX - Step && EndX + Step <= SizeX) \
		{ \
			Check(false, ShouldCheckY, ShouldCheckZ); \
		} \
		else \
		{ \
			Check(true, ShouldCheckY, ShouldCheckZ); \
		}

#define CheckY(ShouldCheckZ) \
		for (uniform int32 Y = StartY; Y < EndY; Y++) \
		{ \
			const uniform float FloatY = Y; \
			\
			if (0 <= Y - Step && Y + Step < SizeY) \
			{ \
				CheckX(false, ShouldCheckZ); \
			} \
			else \
			{ \
				CheckX(true, ShouldCheckZ); \
			} \
		}

		if (0 <= Z - Step && Z + Step < SizeZ)
		{
			CheckY(false);
		}
		else
		{
			CheckY(true);
		}

#undef CheckY
#undef CheckX
#undef Check
#undef CheckNeighbor
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

export void VoxelDistanceFieldUtilities_JumpFlood_ComputeDistances(
	const uniform int32 StartX,
	const uniform int32 StartY,
	const uniform int32 StartZ,
	const uniform int32 EndX,
	const uniform int32 EndY,
	const uniform int32 EndZ,
	const uniform int32 SizeX,
	const uniform int32 SizeY,
	const uniform int32 SizeZ,
	const uniform float InClosestX[],
	const uniform float InClosestY[],
	const uniform float InClosestZ[],
	uniform float OutDistances[])
{
	const uniform int32 SizeXY = SizeX * SizeY;

	for (uniform int32 Z = StartZ; Z < EndZ; Z++)
	{
		for (uniform int32 Y = StartY; Y < EndY; Y++)
		{
			FOREACH(X, StartX, EndX)
			{
				const varying int32 Index = X + SizeX * Y + SizeXY * Z;
				const varying float ClosestX = InClosestX[Index];
				const varying float ClosestY = InClosestY[Index];
				const varying float ClosestZ = InClosestZ[Index];

				if (intbits(ClosestX) == 0xFFFFFFFF)
				{
					OutDistances[Index] = floatbits(0xFFFFFFFF);
					continue;
				}
				check(intbits(ClosestY) != 0xFFFFFFFF);
				check(intbits(ClosestZ) != 0xFFFFFFFF);

				const varying float SquaredDistance =
					Square(ClosestX - X) +
					Square(ClosestY - Y) +
					Square(ClosestZ - Z);

				OutDistances[Index] =
					sqrt(SquaredDistance) *
					(OutDistances[Index] < 0.f ? -1.f : 1.f);

				// TODO Lerp materials using alpha

				//const varying int32 ClosestXi = (int32)round(InClosestX[Index]);
				//const varying int32 ClosestYi = (int32)round(InClosestY[Index]);
				//const varying int32 ClosestZi = (int32)round(InClosestZ[Index]);

				//if (!(0 <= ClosestXi && ClosestXi < SizeX) ||
				//	!(0 <= ClosestYi && ClosestYi < SizeY) ||
				//	!(0 <= ClosestZi && ClosestZi < SizeZ))
				//{
				//	continue;
				//}

				//// TODO Double buffer to avoid read after write?
				//IGNORE_PERF_WARNING
				//OutMaterials[Index] = OutMaterials[ClosestXi + ClosestYi * SizeX + ClosestZi * SizeXY];
			}
		}
	}
}