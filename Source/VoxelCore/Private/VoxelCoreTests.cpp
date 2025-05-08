// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

#if !UE_BUILD_SHIPPING
VOXEL_RUN_ON_STARTUP_GAME()
{
#if !VOXEL_DEBUG
	if (!FVoxelUtilities::IsDevWorkflow())
	{
		return;
	}
#endif

	for (const double Value : TVoxelArray<double>
		{
			0,
			MIN_flt,
			MAX_flt,
			MIN_dbl,
			MAX_dbl,
			0.593970065772997547958216,
			3971.0465808296292259329051626,
			7594.5251824225479095402171066
		})
	{
		FVoxelUtilities::DoubleToFloat_Lower(Value);
		FVoxelUtilities::DoubleToFloat_Higher(Value);

		FVoxelUtilities::DoubleToFloat_Lower(-Value);
		FVoxelUtilities::DoubleToFloat_Higher(-Value);
	}

	{
		TVoxelMap<int32, TSharedPtr<int32>> Map;

		TSharedPtr<int32> Shared = MakeShared<int32>();
		Map.Add_EnsureNew(0, Shared);
		check(Shared);
		Map.Add_EnsureNew(1, MoveTemp(Shared));
		check(!Shared);
	}

	{
		TVoxelSet<int32> Set;
		TVoxelSet<int32> Set2 = Set;
		TVoxelSet<float> Set3 = TVoxelSet<float>(Set);
	}

	{
		TVoxelChunkedSparseArray<int32> Values;
		Values.Add(1);
		for (const int32 Value : Values)
		{
		}
	}

	{
		int64 Sum = 0;
		TVoxelChunkedSparseArray<int32> Values;
		for (int32 Index = 0; Index < 16000; Index++)
		{
			Sum += Index;
			Values.Add(Index);
		}

		for (int32 Index = 0; Index < 1024; Index++)
		{
			const int32 Rand = FMath::RandRange(0, 15599);
			if (Values.IsAllocated_ValidIndex(Rand))
			{
				Sum -= Rand;
				Values.RemoveAt(Rand);
			}
		}

		int64 NewSum = 0;
		for (const int32 Value : Values)
		{
			NewSum += Value;
		}
		check(Sum == NewSum);
	}
}
#endif