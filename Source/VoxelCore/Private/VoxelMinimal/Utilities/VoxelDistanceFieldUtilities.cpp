// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Misc/ScopedSlowTask.h"
#include "VoxelDistanceFieldUtilitiesImpl.ispc.generated.h"

void FVoxelUtilities::JumpFlood(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances,
	TVoxelArray<float>& OutClosestX,
	TVoxelArray<float>& OutClosestY,
	TVoxelArray<float>& OutClosestZ)
{
	VOXEL_FUNCTION_COUNTER();

	const int64 SizeXYZ = Size.X * Size.Y * Size.Z;
	if (!ensureVoxelSlow(SizeXYZ < 1024 * 1024 * 1024))
	{
		return;
	}

	{
		VOXEL_SCOPE_COUNTER("SetNumFast");

		SetNumFast(OutClosestX, SizeXYZ);
		SetNumFast(OutClosestY, SizeXYZ);
		SetNumFast(OutClosestZ, SizeXYZ);
	}

	{
		VOXEL_SCOPE_COUNTER("Initialize");

		FVoxelParallelTaskScope Scope;

		for (int32 Z = 0; Z < Size.Z; Z++)
		{
			Scope.AddTask([&, Z]
			{
				VOXEL_SCOPE_COUNTER_FORMAT("FVoxelUtilities::JumpFlood Initialize Num=%d", Size.X * Size.Y);

				ispc::VoxelDistanceFieldUtilities_JumpFlood_Initialize(
					Z,
					Size.X,
					Size.Y,
					Size.Z,
					Distances.GetData(),
					OutClosestX.GetData(),
					OutClosestY.GetData(),
					OutClosestZ.GetData());
			});
		}
	}

	JumpFlood_Initialized(
		Size,
		Distances,
		OutClosestX,
		OutClosestY,
		OutClosestZ);
}

void FVoxelUtilities::JumpFlood_Initialized(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances,
	TVoxelArray<float>& ClosestX,
	TVoxelArray<float>& ClosestY,
	TVoxelArray<float>& ClosestZ)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("JumpFlood %dx%dx%d Num=%d", Size.X, Size.Y, Size.Z, Size.X * Size.Y * Size.Z);

	const int64 SizeXYZ = Size.X * Size.Y * Size.Z;
	if (!ensureVoxelSlow(SizeXYZ < 1024 * 1024 * 1024))
	{
		return;
	}

	check(ClosestX.Num() == SizeXYZ);
	check(ClosestY.Num() == SizeXYZ);
	check(ClosestZ.Num() == SizeXYZ);

	TVoxelArray<float> ClosestXTemp;
	TVoxelArray<float> ClosestYTemp;
	TVoxelArray<float> ClosestZTemp;
	{
		VOXEL_SCOPE_COUNTER("SetNumFast");

		SetNumFast(ClosestXTemp, SizeXYZ);
		SetNumFast(ClosestYTemp, SizeXYZ);
		SetNumFast(ClosestZTemp, SizeXYZ);
	}

	{
		VOXEL_SCOPE_COUNTER("Initialize ClosestTemp");

		Voxel::ParallelFor(ClosestXTemp, [&](const TVoxelArrayView<float> View)
		{
			SetAll(View, NaNf());
		});
		Voxel::ParallelFor(ClosestYTemp, [&](const TVoxelArrayView<float> View)
		{
			SetAll(View, NaNf());
		});
		Voxel::ParallelFor(ClosestZTemp, [&](const TVoxelArrayView<float> View)
		{
			SetAll(View, NaNf());
		});
	}

	const int32 NumPasses = FMath::Max<int32>(1, FMath::FloorLog2(Size.GetMax()));

	TVoxelOptional<FScopedSlowTask> SlowTask;
	if (IsInGameThread())
	{
		SlowTask.Emplace(NumPasses, INVTEXT("Jump Flood"));
	}

	for (int32 Pass = 0; Pass < NumPasses; Pass++)
	{
		if (SlowTask)
		{
			SlowTask->EnterProgressFrame();

			if (SlowTask->ShouldCancel())
			{
				return;
			}
		}

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
					Z,
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

	{
		VOXEL_SCOPE_COUNTER("ComputeDistances");

		FVoxelParallelTaskScope Scope;

		for (int32 Z = 0; Z < Size.Z; Z++)
		{
			Scope.AddTask([&, Z]
			{
				VOXEL_SCOPE_COUNTER_FORMAT("FVoxelUtilities::JumpFlood ComputeDistances Num=%d", Size.X * Size.Y);

				ispc::VoxelDistanceFieldUtilities_JumpFlood_ComputeDistances(
					Z,
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
		ClosestXTemp = MakeSharedCopy(MoveTemp(ClosestXTemp)),
		ClosestYTemp = MakeSharedCopy(MoveTemp(ClosestYTemp)),
		ClosestZTemp = MakeSharedCopy(MoveTemp(ClosestZTemp))]
	{
		VOXEL_SCOPE_COUNTER("FVoxelUtilities::JumpFlood Free Closest");

		ClosestXTemp->Reset();
		ClosestYTemp->Reset();
		ClosestZTemp->Reset();
	});
}

void FVoxelUtilities::JumpFlood(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<float> ClosestX;
	TVoxelArray<float> ClosestY;
	TVoxelArray<float> ClosestZ;

	JumpFlood(
		Size,
		Distances,
		ClosestX,
		ClosestY,
		ClosestZ);

	Voxel::AsyncTask([
		ClosestX = MakeSharedCopy(MoveTemp(ClosestX)),
		ClosestY = MakeSharedCopy(MoveTemp(ClosestY)),
		ClosestZ = MakeSharedCopy(MoveTemp(ClosestZ))]
	{
		VOXEL_SCOPE_COUNTER("FVoxelUtilities::JumpFlood Free Closest");

		ClosestX->Reset();
		ClosestY->Reset();
		ClosestZ->Reset();
	});
}