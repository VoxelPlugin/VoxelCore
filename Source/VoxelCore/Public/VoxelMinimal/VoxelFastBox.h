// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

struct VOXELCORE_API FVoxelFastBox
{
	VectorRegister4f Min = VectorZeroFloat();
	VectorRegister4f Max = VectorZeroFloat();

	FVoxelFastBox() = default;

	FORCEINLINE explicit FVoxelFastBox(
		const FVector3f& Min,
		const FVector3f& Max)
		: Min(VectorLoadFloat3(&Min))
		, Max(VectorLoadFloat3(&Max))
	{
	}
	FORCEINLINE explicit FVoxelFastBox(const FVoxelBox& Bounds)
		: FVoxelFastBox(
			FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min),
			FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max))
	{
	}

	FORCEINLINE FVector3f GetMin() const
	{
		FVector3f Result;
		VectorStoreFloat3(Min, &Result);
		return Result;
	}
	FORCEINLINE FVector3f GetMax() const
	{
		FVector3f Result;
		VectorStoreFloat3(Max, &Result);
		return Result;
	}
	FORCEINLINE FVoxelBox GetBox() const
	{
		return FVoxelBox(GetMin(), GetMax());
	}

	FORCEINLINE bool Intersects(const FVoxelFastBox& Other) const
	{
		const int32 Mask = VectorMaskBits(VectorBitwiseOr(
			VectorCompareLT(Max, Other.Min),
			VectorCompareLT(Other.Max, Min)));

		const bool bIntersects = (Mask & 0b111) == 0;
		checkVoxelSlow(bIntersects == GetBox().Intersects(Other.GetBox()));
		return bIntersects;
	}
	FORCEINLINE FVoxelFastBox UnionWith(const FVoxelFastBox& Other) const
	{
		FVoxelFastBox Result;
		Result.Min = VectorMin(Min, Other.Min);
		Result.Max = VectorMax(Max, Other.Max);
		return Result;
	}
};