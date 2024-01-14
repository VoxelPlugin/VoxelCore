// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

template<typename T>
struct TVoxelWelfordVariance
{
	T Average = FVoxelUtilities::MakeSafe<T>();
	T ScaledVariance = FVoxelUtilities::MakeSafe<T>();
	int32 Num = 0;

	FORCEINLINE void Add(const T& Value)
	{
		Num++;

		const T Delta = Value - Average;
		Average += Delta / Num;
		ScaledVariance += Delta * (Value - Average);
	}

	FORCEINLINE T GetVariance() const
	{
		ensureVoxelSlow(Num > 0);
		if (Num <= 1)
		{
			return FVoxelUtilities::MakeSafe<T>();
		}
		else
		{
			return ScaledVariance / (Num - 1);
		}
	}
	FORCEINLINE T GetStd() const
	{
		return FMath::Sqrt(GetVariance());
	}
};