// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

// +/- 1024: prevents integers overflow
const FVoxelIntInterval FVoxelIntInterval::Infinite = FVoxelIntInterval(MIN_int32 + 1024, MAX_int32 - 1024);
const FVoxelIntInterval FVoxelIntInterval::InvertedInfinite = []
{
	FVoxelIntInterval Result;
	Result.Min = Infinite.Max;
	Result.Max = Infinite.Min;
	return Result;
}();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelIntInterval FVoxelIntInterval::FromValues(const TConstVoxelArrayView<int32> Values)
{
	if (Values.Num() == 0)
	{
		return {};
	}

	const FInt32Interval Result = FVoxelUtilities::GetMinMax(Values);
	return FVoxelIntInterval(Result.Min, Result.Max + 1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelIntInterval::ToString() const
{
	return FString::Printf(TEXT("[%d-%d]"), Min, Max);
}