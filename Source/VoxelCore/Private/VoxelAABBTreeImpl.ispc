// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.isph"
#include "VoxelWelfordVariance.isph"

export void VoxelAABBTree_ComputeVariance(
	const uniform float MinX[],
	const uniform float MinY[],
	const uniform float MinZ[],
	const uniform float MaxX[],
	const uniform float MaxY[],
	const uniform float MaxZ[],
	const uniform int32 Num,
	uniform float& OutMinX,
	uniform float& OutMinY,
	uniform float& OutMinZ,
	uniform float& OutMaxX,
	uniform float& OutMaxY,
	uniform float& OutMaxZ,
	uniform float& OutAverageX,
	uniform float& OutAverageY,
	uniform float& OutAverageZ,
	uniform float& OutVarianceX,
	uniform float& OutVarianceY,
	uniform float& OutVarianceZ)
{
	const uniform int32 AlignedNum = programCount * (Num / programCount);

	FVoxelWelfordVariance VarianceX;
	FVoxelWelfordVariance VarianceY;
	FVoxelWelfordVariance VarianceZ;
	InitializeVariance(VarianceX);
	InitializeVariance(VarianceY);
	InitializeVariance(VarianceZ);

	varying float RootMinX = MAX_flt;
	varying float RootMinY = MAX_flt;
	varying float RootMinZ = MAX_flt;
	varying float RootMaxX = -MAX_flt;
	varying float RootMaxY = -MAX_flt;
	varying float RootMaxZ = -MAX_flt;

	for (uniform int32 Index = 0; Index < AlignedNum; Index += programCount)
	{
		RootMinX = min(RootMinX, MinX[Index + programIndex]);
		RootMinY = min(RootMinY, MinY[Index + programIndex]);
		RootMinZ = min(RootMinZ, MinZ[Index + programIndex]);
		RootMaxX = max(RootMaxX, MaxX[Index + programIndex]);
		RootMaxY = max(RootMaxY, MaxY[Index + programIndex]);
		RootMaxZ = max(RootMaxZ, MaxZ[Index + programIndex]);

		const varying float X = (MinX[Index + programIndex] + MaxX[Index + programIndex]) / 2.f;
		const varying float Y = (MinY[Index + programIndex] + MaxY[Index + programIndex]) / 2.f;
		const varying float Z = (MinZ[Index + programIndex] + MaxZ[Index + programIndex]) / 2.f;

		AddVariance(VarianceX, X);
		AddVariance(VarianceY, Y);
		AddVariance(VarianceZ, Z);
	}

	for (uniform int32 Index = AlignedNum; Index < Num; Index++)
	{
		RootMinX = min(RootMinX, MinX[Index]);
		RootMinY = min(RootMinY, MinY[Index]);
		RootMinZ = min(RootMinZ, MinZ[Index]);
		RootMaxX = max(RootMaxX, MaxX[Index]);
		RootMaxY = max(RootMaxY, MaxY[Index]);
		RootMaxZ = max(RootMaxZ, MaxZ[Index]);

		const uniform float X = (MinX[Index] + MaxX[Index]) / 2.f;
		const uniform float Y = (MinY[Index] + MaxY[Index]) / 2.f;
		const uniform float Z = (MinZ[Index] + MaxZ[Index]) / 2.f;

		AddVariance(VarianceX, X);
		AddVariance(VarianceY, Y);
		AddVariance(VarianceZ, Z);
	}

	OutMinX = reduce_min(RootMinX);
	OutMinY = reduce_min(RootMinY);
	OutMinZ = reduce_min(RootMinZ);
	OutMaxX = reduce_max(RootMaxX);
	OutMaxY = reduce_max(RootMaxY);
	OutMaxZ = reduce_max(RootMaxZ);

	OutAverageX = GetUniformAverage(VarianceX);
	OutAverageY = GetUniformAverage(VarianceY);
	OutAverageZ = GetUniformAverage(VarianceZ);

	OutVarianceX = GetUniformVariance(VarianceX);
	OutVarianceY = GetUniformVariance(VarianceY);
	OutVarianceZ = GetUniformVariance(VarianceZ);
}

export uniform int32 VoxelAABBTree_Split_X(
	uniform int32 Payload[],
	uniform float MinX[],
	uniform float MinY[],
	uniform float MinZ[],
	uniform float MaxX[],
	uniform float MaxY[],
	uniform float MaxZ[],
	const uniform float SplitValue,
	const uniform int32 StartIndex,
	const uniform int32 EndIndex,
	const uniform int32 Max)
{
	const uniform float SplitValuesTimes2 = 2 * SplitValue;

	uniform int32 Index0 = StartIndex;
	uniform int32 Index1 = EndIndex - 1;

#if VOXEL_DEBUG
#define CHECK_INDICES() \
	for (uniform int32 Index = StartIndex; Index < Index0; Index++) \
	{ \
		check((MinX[Index] + MaxX[Index]) <= SplitValuesTimes2); \
	} \
	for (uniform int32 Index = Index1 + 1; Index < EndIndex; Index++) \
	{ \
		check((MinX[Index] + MaxX[Index]) > SplitValuesTimes2); \
	}

#else
#define CHECK_INDICES()
#endif

	CHECK_INDICES();

	if (Index1 - Index0 >= 2 * programCount)
	{
		check(Index0 + programCount <= EndIndex);
		check(Index1 < EndIndex);

		varying float Min0 = MinX[Index0 + programIndex];
		varying float Max0 = MaxX[Index0 + programIndex];

		varying float Min1 = MinX[Index1 - (programCount - 1) + programIndex];
		varying float Max1 = MaxX[Index1 - (programCount - 1) + programIndex];

	FetchIndex0:
		{
			CHECK_INDICES();

			const uniform int32 NumToSkip = count_trailing_zeros(packmask((Min0 + Max0) > SplitValuesTimes2));

			if (NumToSkip >= programCount)
			{
				Index0 += programCount;
				CHECK_INDICES();

				if (Index1 - Index0 < 2 * programCount)
				{
					goto Finalize;
				}

				check(Index0 + programCount <= EndIndex);

				Min0 = MinX[Index0 + programIndex];
				Max0 = MaxX[Index0 + programIndex];

				goto FetchIndex0;
			}

			if (NumToSkip != 0)
			{
				Index0 += NumToSkip;
				CHECK_INDICES();

				if (Index1 - Index0 < 2 * programCount)
				{
					goto Finalize;
				}

				check(Index0 + programCount <= EndIndex);

				Min0 = MinX[Index0 + programIndex];
				Max0 = MaxX[Index0 + programIndex];
			}
		}
		check(extract((Min0 + Max0), 0) > SplitValuesTimes2);

	FetchIndex1:
		{
			CHECK_INDICES();

			const uniform int32 Mask = packmask((Min1 + Max1) <= SplitValuesTimes2);
			const uniform int32 NumToSkip = count_leading_zeros(Mask) - (32 - programCount);

			if (NumToSkip == programCount)
			{
				Index1 -= programCount;
				CHECK_INDICES();

				if (Index1 - Index0 < 2 * programCount)
				{
					goto Finalize;
				}

				check(Index1 < EndIndex);

				Min1 = MinX[Index1 - (programCount - 1) + programIndex];
				Max1 = MaxX[Index1 - (programCount - 1) + programIndex];

				goto FetchIndex1;
			}

			if (NumToSkip != 0)
			{
				Index1 -= NumToSkip;
				CHECK_INDICES();

				if (Index1 - Index0 < 2 * programCount)
				{
					goto Finalize;
				}

				check(Index1 < EndIndex);

				Min1 = MinX[Index1 - (programCount - 1) + programIndex];
				Max1 = MaxX[Index1 - (programCount - 1) + programIndex];
			}
		}
		check(extract((Min1 + Max1), programCount - 1) <= SplitValuesTimes2);

#if TARGET_WIDTH == 8
		const varying int32 Shuffle = MakeRegister(7, 6, 5, 4, 3, 2, 1, 0);
		const varying int32 Indices = MakeRegister(0, 1, 2, 3, 4, 5, 6, 7);
#elif TARGET_WIDTH == 4
		const varying int32 Shuffle = MakeRegister(3, 2, 1, 0);
		const varying int32 Indices = MakeRegister(0, 1, 2, 3);
#else
#error "Unsupported TARGET_WIDTH"
#endif

		const varying bool Mask0 = (Min0 + Max0) <= SplitValuesTimes2;
		const varying bool Mask1 = shuffle(Min1 + Max1, Shuffle) > SplitValuesTimes2;
		const uniform int32 NumToMove = min(programCount, (uniform int32)count_trailing_zeros(packmask(or(Mask0, Mask1))));
		check(NumToMove > 0);
		check(NumToMove <= programCount);

		const varying bool Mask = Indices < NumToMove;
		check(extract(Mask, 0));

		check(Index0 + programCount <= EndIndex);

		const varying int32 LocalPayload = Payload[Index0 + programIndex];
		const varying float LocalMinX = MinX[Index0 + programIndex];
		const varying float LocalMinY = MinY[Index0 + programIndex];
		const varying float LocalMinZ = MinZ[Index0 + programIndex];
		const varying float LocalMaxX = MaxX[Index0 + programIndex];
		const varying float LocalMaxY = MaxY[Index0 + programIndex];
		const varying float LocalMaxZ = MaxZ[Index0 + programIndex];

		const uniform int32 DataIndex1 = Index1 - (NumToMove - 1);

		check(DataIndex1 + programCount <= Max);

#define COPY(Array) Array[Index0 + programIndex] = select(Mask, Array[DataIndex1 + programIndex], Array[Index0 + programIndex]);

		COPY(Payload);
		COPY(MinX);
		COPY(MinY);
		COPY(MinZ);
		COPY(MaxX);
		COPY(MaxY);
		COPY(MaxZ);

#undef COPY

#define COPY(Array) Array[DataIndex1 + programIndex] = select(Mask, Local ## Array, Array[DataIndex1 + programIndex]);

		COPY(Payload);
		COPY(MinX);
		COPY(MinY);
		COPY(MinZ);
		COPY(MaxX);
		COPY(MaxY);
		COPY(MaxZ);

#undef COPY

		Index0 += NumToMove;
		Index1 -= NumToMove;

		CHECK_INDICES();

		if (Index1 - Index0 < 2 * programCount)
		{
			goto Finalize;
		}

		check(Index0 + programCount <= EndIndex);
		check(Index1 < EndIndex);

		Min0 = MinX[Index0 + programIndex];
		Max0 = MaxX[Index0 + programIndex];

		Min1 = MinX[Index1 - (programCount - 1) + programIndex];
		Max1 = MaxX[Index1 - (programCount - 1) + programIndex];

		goto FetchIndex0;
	}

Finalize:
	CHECK_INDICES();

	while (Index0 < Index1)
	{
		check(0 <= Index0 && Index0 < EndIndex);
		check(0 <= Index1 && Index1 < EndIndex);

		if ((MinX[Index0] + MaxX[Index0]) <= SplitValuesTimes2)
		{
			Index0++;
			CHECK_INDICES();
			continue;
		}
		if ((MinX[Index1] + MaxX[Index1]) > SplitValuesTimes2)
		{
			Index1--;
			CHECK_INDICES();
			continue;
		}

#define SWAP(Type, Array) { const uniform Type Local = Array[Index0]; Array[Index0] = Array[Index1]; Array[Index1] = Local; }

		SWAP(int32, Payload);
		SWAP(float, MinX);
		SWAP(float, MinY);
		SWAP(float, MinZ);
		SWAP(float, MaxX);
		SWAP(float, MaxY);
		SWAP(float, MaxZ);

#undef SWAP

		check((MinX[Index0] + MaxX[Index0]) <= SplitValuesTimes2);
		check((MinX[Index1] + MaxX[Index1]) > SplitValuesTimes2);

		Index0++;
		Index1--;

		CHECK_INDICES();
	}

	check(0 <= Index0 && Index0 < EndIndex);

	return (MinX[Index0] + MaxX[Index0]) <= SplitValuesTimes2 ? Index0 + 1 : Index0;
}