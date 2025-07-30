// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

const FVoxelInterval FVoxelInterval::Infinite = FVoxelInterval(-1e30, 1e30);
const FVoxelInterval FVoxelInterval::InvertedInfinite = []
{
	FVoxelInterval Result;
	Result.Min = 1e30;
	Result.Max = -1e30;
	return Result;
}();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInterval FVoxelInterval::FromValues(const TConstVoxelArrayView<float> Values)
{
	if (Values.Num() == 0)
	{
		return {};
	}

	const FFloatInterval Result = FVoxelUtilities::GetMinMax(Values);
	return FVoxelInterval(Result.Min, Result.Max);
}

FVoxelInterval FVoxelInterval::FromValues(const TConstVoxelArrayView<double> Values)
{
	if (Values.Num() == 0)
	{
		return {};
	}

	const FDoubleInterval Result = FVoxelUtilities::GetMinMax(Values);
	return FVoxelInterval(Result.Min, Result.Max);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelInterval::ToString() const
{
	return FString::Printf(TEXT("[%f-%f]"), Min, Max);
}