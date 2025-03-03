// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelDistanceFieldUtilitiesImpl.ispc.generated.h"

void FVoxelUtilities::JumpFlood(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances,
	TVoxelArray<float>& OutClosestX,
	TVoxelArray<float>& OutClosestY,
	TVoxelArray<float>& OutClosestZ)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("JumpFlood %dx%dx%d Num=%d", Size.X, Size.Y, Size.Z, Size.X * Size.Y * Size.Z);

	TVoxelArray<float> ClosestX;
	TVoxelArray<float> ClosestY;
	TVoxelArray<float> ClosestZ;
	SetNumFast(ClosestX, Size.X * Size.Y * Size.Z);
	SetNumFast(ClosestY, Size.X * Size.Y * Size.Z);
	SetNumFast(ClosestZ, Size.X * Size.Y * Size.Z);

	{
		VOXEL_SCOPE_COUNTER("Initialize");

		FVoxelParallelTaskScope Scope;

		for (int32 Z = 0; Z < Size.Z; Z++)
		{
			Scope.AddTask([&, Z]
			{
				VOXEL_SCOPE_COUNTER_FORMAT("FVoxelUtilities::JumpFlood Initialize Num=%d", Size.X * Size.Y);

				ispc::VoxelDistanceFieldUtilities_JumpFlood_Initialize(
					0,
					0,
					Z,
					Size.X,
					Size.Y,
					Z + 1,
					Size.X,
					Size.Y,
					Size.Z,
					Distances.GetData(),
					ClosestX.GetData(),
					ClosestY.GetData(),
					ClosestZ.GetData());
			});
		}
	}

	{
		TVoxelArray<float> ClosestXTemp;
		TVoxelArray<float> ClosestYTemp;
		TVoxelArray<float> ClosestZTemp;
		{
			VOXEL_SCOPE_COUNTER("Initialize ClosestTemp");

			SetNumFast(ClosestXTemp, Size.X * Size.Y * Size.Z);
			SetNumFast(ClosestYTemp, Size.X * Size.Y * Size.Z);
			SetNumFast(ClosestZTemp, Size.X * Size.Y * Size.Z);

			ParallelFor(ClosestXTemp, [&](const TVoxelArrayView<float> View)
			{
				SetAll(View, NaNf());
			});
			ParallelFor(ClosestYTemp, [&](const TVoxelArrayView<float> View)
			{
				SetAll(View, NaNf());
			});
			ParallelFor(ClosestZTemp, [&](const TVoxelArrayView<float> View)
			{
				SetAll(View, NaNf());
			});
		}

		const int32 NumPasses = FMath::Max<int32>(1, FMath::FloorLog2(Size.GetMax()));

		for (int32 Pass = 0; Pass < NumPasses; Pass++)
		{
			// -1: we want to start with half the size
			const int32 Step = 1 << (NumPasses - 1 - Pass);

			ON_SCOPE_EXIT
			{
				Swap(ClosestX, ClosestXTemp);
				Swap(ClosestY, ClosestYTemp);
				Swap(ClosestZ, ClosestZTemp);
			};

			VOXEL_SCOPE_COUNTER_FORMAT("JumpFlood Step=%d", Step);

			FVoxelParallelTaskScope Scope;

			for (int32 Z = 0; Z < Size.Z; Z++)
			{
				Scope.AddTask([&, Z]
				{
					VOXEL_SCOPE_COUNTER_FORMAT("FVoxelUtilities::JumpFlood JumpFlood Num=%d", Size.X * Size.Y);

					ispc::VoxelDistanceFieldUtilities_JumpFlood_JumpFlood(
						0,
						0,
						Z,
						Size.X,
						Size.Y,
						Z + 1,
						Size.X,
						Size.Y,
						Size.Z,
						Step,
						ClosestX.GetData(),
						ClosestY.GetData(),
						ClosestZ.GetData(),
						ClosestXTemp.GetData(),
						ClosestYTemp.GetData(),
						ClosestZTemp.GetData());
				});
			}
		}

		Voxel::AsyncTask([
			ClosestXTemp = MakeSharedCopy(MoveTemp(ClosestXTemp)),
			ClosestYTemp = MakeSharedCopy(MoveTemp(ClosestYTemp)),
			ClosestZTemp = MakeSharedCopy(MoveTemp(ClosestZTemp))]
		{
			VOXEL_SCOPE_COUNTER("Free ClosestTemp");

			ClosestXTemp->Reset();
			ClosestYTemp->Reset();
			ClosestZTemp->Reset();
		});
	}

	{
		VOXEL_SCOPE_COUNTER("ComputeDistances");

		FVoxelParallelTaskScope Scope;

		for (int32 Z = 0; Z < Size.Z; Z++)
		{
			Scope.AddTask([&, Z]
			{
				VOXEL_SCOPE_COUNTER_FORMAT("FVoxelUtilities::JumpFlood ComputeDistances Num=%d", Size.X * Size.Y);

				ispc::VoxelDistanceFieldUtilities_JumpFlood_ComputeDistances(
					0,
					0,
					Z,
					Size.X,
					Size.Y,
					Z + 1,
					Size.X,
					Size.Y,
					Size.Z,
					ClosestX.GetData(),
					ClosestY.GetData(),
					ClosestZ.GetData(),
					Distances.GetData());
			});
		}
	}

	Voxel::AsyncTask([
		ClosestX = MakeSharedCopy(MoveTemp(ClosestX)),
		ClosestY = MakeSharedCopy(MoveTemp(ClosestY)),
		ClosestZ = MakeSharedCopy(MoveTemp(ClosestZ))]
	{
		VOXEL_SCOPE_COUNTER("Free Closest");

		ClosestX->Reset();
		ClosestY->Reset();
		ClosestZ->Reset();
	});

	OutClosestX = MoveTemp(ClosestX);
	OutClosestY = MoveTemp(ClosestY);
	OutClosestZ = MoveTemp(ClosestZ);
}