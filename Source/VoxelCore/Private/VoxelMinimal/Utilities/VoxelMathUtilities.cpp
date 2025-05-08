// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

int32 FVoxelUtilities::GetNumberOfDigits(const int32 Value)
{
	return GetNumberOfDigits(int64(Value));
}

int32 FVoxelUtilities::GetNumberOfDigits(int64 Value)
{
	Value = FMath::Abs(Value);

	if (Value == 0)
	{
		return 1;
	}

	return FMath::FloorToInt(FMath::LogX(10., double(Value)) + 1);
}