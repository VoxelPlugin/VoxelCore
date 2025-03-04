// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelDistanceFieldUtilitiesImpl.ispc.generated.h"

void JumpFloodImpl(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances,
	TVoxelArray<float>* OutClosestX,
	TVoxelArray<float>* OutClosestY,
	TVoxelArray<float>* OutClosestZ)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("JumpFlood %dx%dx%d Num=%d", Size.X, Size.Y, Size.Z, Size.X * Size.Y * Size.Z);

	const int64 SizeXYZ = Size.X * Size.Y * Size.Z;
	if (!ensureVoxelSlow(SizeXYZ < 1024 * 1024 * 1024))
	{
		return;
	}

	TVoxelArray<float> ClosestX;
	TVoxelArray<float> ClosestY;
	TVoxelArray<float> ClosestZ;

	TVoxelArray<float> ClosestXTemp;
	TVoxelArray<float> ClosestYTemp;
	TVoxelArray<float> ClosestZTemp;

	{
		VOXEL_SCOPE_COUNTER("SetNumFast");

		FVoxelUtilities::SetNumFast(ClosestX, SizeXYZ);
		FVoxelUtilities::SetNumFast(ClosestY, SizeXYZ);
		FVoxelUtilities::SetNumFast(ClosestZ, SizeXYZ);

		FVoxelUtilities::SetNumFast(ClosestXTemp, SizeXYZ);
		FVoxelUtilities::SetNumFast(ClosestYTemp, SizeXYZ);
		FVoxelUtilities::SetNumFast(ClosestZTemp, SizeXYZ);
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
					ClosestX.GetData(),
					ClosestY.GetData(),
					ClosestZ.GetData());
			});
		}
	}

	{
		{
			VOXEL_SCOPE_COUNTER("Initialize ClosestTemp");

			ParallelFor(ClosestXTemp, [&](const TVoxelArrayView<float> View)
			{
				FVoxelUtilities::SetAll(View, FVoxelUtilities::NaNf());
			});
			ParallelFor(ClosestYTemp, [&](const TVoxelArrayView<float> View)
			{
				FVoxelUtilities::SetAll(View, FVoxelUtilities::NaNf());
			});
			ParallelFor(ClosestZTemp, [&](const TVoxelArrayView<float> View)
			{
				FVoxelUtilities::SetAll(View, FVoxelUtilities::NaNf());
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

	if (OutClosestX)
	{
		*OutClosestX = MoveTemp(ClosestX);
	}
	if (OutClosestY)
	{
		*OutClosestY = MoveTemp(ClosestY);
	}
	if (OutClosestZ)
	{
		*OutClosestZ = MoveTemp(ClosestZ);
	}

	Voxel::AsyncTask([
		ClosestX = MakeSharedCopy(MoveTemp(ClosestX)),
		ClosestY = MakeSharedCopy(MoveTemp(ClosestY)),
		ClosestZ = MakeSharedCopy(MoveTemp(ClosestZ)),
		ClosestXTemp = MakeSharedCopy(MoveTemp(ClosestXTemp)),
		ClosestYTemp = MakeSharedCopy(MoveTemp(ClosestYTemp)),
		ClosestZTemp = MakeSharedCopy(MoveTemp(ClosestZTemp))]
	{
		VOXEL_SCOPE_COUNTER("Free Closest");

		ClosestX->Reset();
		ClosestY->Reset();
		ClosestZ->Reset();

		ClosestXTemp->Reset();
		ClosestYTemp->Reset();
		ClosestZTemp->Reset();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::JumpFlood(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances,
	TVoxelArray<float>& OutClosestX,
	TVoxelArray<float>& OutClosestY,
	TVoxelArray<float>& OutClosestZ)
{
	JumpFloodImpl(
		Size,
		Distances,
		&OutClosestX,
		&OutClosestY,
		&OutClosestZ);
}

void FVoxelUtilities::JumpFlood(
	const FIntVector& Size,
	const TVoxelArrayView<float> Distances)
{
	JumpFloodImpl(
		Size,
		Distances,
		nullptr,
		nullptr,
		nullptr);
}