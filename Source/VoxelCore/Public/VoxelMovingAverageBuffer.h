// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

// https://mveg.es/posts/fast-numerically-stable-moving-average/
struct FVoxelMovingAverageBuffer
{
	explicit FVoxelMovingAverageBuffer(const int32 ValuesCount)
		: WindowSize(1 << FMath::CeilLogTwo(ValuesCount))
	{
		ensure(FMath::IsPowerOfTwo(ValuesCount));
		FVoxelUtilities::SetNum(Values, WindowSize * 2);
	}

	void AddValue(const double NewValue)
	{
		const int32 PositionIndex = WindowSize - 1 + Position;
		Position = (Position + 1) % WindowSize;

		Values[PositionIndex] = NewValue;

		// Update parents
		for (int32 Index = PositionIndex; Index > 0; Index = GetParentIndex(Index))
		{
			const int32 ParentToUpdate = GetParentIndex(Index);
			Values[ParentToUpdate] = Values[GetLeftChildIndex(ParentToUpdate)] + Values[GetRightChildIndex(ParentToUpdate)];
		}
	}

	FORCEINLINE double GetAverageValue() const
	{
		return Values[0] / WindowSize;
	}

	FORCEINLINE int32 GetWindowSize() const
	{
		return WindowSize;
	}

private:
	static int32 GetParentIndex(const int32 ChildIndex)
	{
		check(ChildIndex > 0);
		return (ChildIndex - 1) / 2;
	}

	static int32 GetLeftChildIndex(const int32 ParentIndex)
	{
		return 2 * ParentIndex + 1;
	}

	static int32 GetRightChildIndex(const int32 ParentIndex)
	{
		return 2 * ParentIndex + 2;
	}

private:
	int32 WindowSize = 0;
	int32 Position = 0;
	TArray<double> Values;
};