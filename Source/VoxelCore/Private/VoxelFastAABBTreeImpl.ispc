// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.isph"

export void VoxelFastAABBTree_Compute(
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

		// Not entirely accurate as the last few values will be over-represented x8, but that's fine
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