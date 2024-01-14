// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelColumnSizeData.h"

void FVoxelColumnSizeData::SetValueWidth(
	float NewWidth,
	const int32 Indentation,
	const float CurrentSize)
{
	if (CurrentSize != 0.f &&
		Indentation != 0)
	{
		NewWidth /= (CurrentSize + Indentation * IndentationValue) / CurrentSize;
	}

	ensure(NewWidth <= 1.0f);
	ValueWidth = NewWidth;
}

float FVoxelColumnSizeData::GetNameWidth(
	const int32 Indentation, 
	const float CurrentSize) const
{
	return 1.f - GetValueWidth(Indentation, CurrentSize);
}

float FVoxelColumnSizeData::GetValueWidth(
	const int32 Indentation,
	const float CurrentSize) const
{
	if (CurrentSize == 0.f ||
		Indentation == 0)
	{
		return ValueWidth;
	}

	return (CurrentSize + Indentation * IndentationValue) / CurrentSize * ValueWidth;
}