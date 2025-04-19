// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

namespace FVoxelUtilities
{
	FORCEINLINE FLinearColor GetDistanceFieldColor(const float Value)
	{
		// Credit for this snippet goes to Inigo Quilez

		FLinearColor Color = FLinearColor::White - FMath::Sign(Value) * FLinearColor(0.1f, 0.4f, 0.7f, 0.f);
		Color *= 1.0f - FMath::Exp(-3.f * FMath::Abs(Value));
		Color *= 0.8f + 0.2f * FMath::Cos(150.f * Value);
		Color = FMath::Lerp(Color, FLinearColor::White, 1.0 - FMath::SmoothStep(0.0f, 0.01f, FMath::Abs(Value)));
		Color.A = 1.f;
		return Color;
	}

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	FORCEINLINE float GetSmoothMinAlpha(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness)
	{
		return FMath::Clamp(0.5f - 0.5f * (DistanceB - DistanceA) / Smoothness, 0.0f, 1.0f);
	}
	FORCEINLINE float GetSmoothMaxAlpha(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness)
	{
		return FMath::Clamp(0.5f + 0.5f * (DistanceB - DistanceA) / Smoothness, 0.0f, 1.0f);
	}

	FORCEINLINE float SmoothMinFromAlpha(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness,
		const float Alpha)
	{
		return FMath::Lerp(DistanceA, DistanceB, Alpha) - Smoothness * Alpha * (1.0f - Alpha);
	}
	FORCEINLINE float SmoothMaxFromAlpha(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness,
		const float Alpha)
	{
		return FMath::Lerp(DistanceA, DistanceB, Alpha) + Smoothness * Alpha * (1.0f - Alpha);
	}

	FORCEINLINE float SmoothMin(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness)
	{
		return SmoothMinFromAlpha(
			DistanceA,
			DistanceB,
			Smoothness,
			GetSmoothMinAlpha(DistanceA, DistanceB, Smoothness));
	}
	FORCEINLINE float SmoothMax(
		const float DistanceA,
		const float DistanceB,
		const float Smoothness)
	{
		return SmoothMaxFromAlpha(
			DistanceA,
			DistanceB,
			Smoothness,
			GetSmoothMaxAlpha(DistanceA, DistanceB, Smoothness));
	}

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	FORCEINLINE void CombineScaleAndOffset(
		const float ScaleA,
		const float OffsetA,
		const float ScaleB,
		const float OffsetB,
		float& OutScale,
		float& OutOffset)
	{
		// Height = (Height * ScaleA + OffsetA) * ScaleB + OffsetB
		// Height = Height * ScaleA * ScaleB + OffsetA * ScaleB + OffsetB

		OutScale = ScaleA * ScaleB;
		OutOffset = OffsetA * ScaleB + OffsetB;
	}
	FORCEINLINE void CombineScaleAndOffset(
		const float ScaleA,
		const float OffsetA,
		const float ScaleB,
		const float OffsetB,
		const float ScaleC,
		const float OffsetC,
		float& OutScale,
		float& OutOffset)
	{
		// Height = ((Height * ScaleA + OffsetA) * ScaleB + OffsetB) * ScaleC + OffsetC
		// Height = (Height * ScaleA * ScaleB + OffsetA * ScaleB + OffsetB) * ScaleC + OffsetC
		// Height = Height * ScaleA * ScaleB * ScaleC + OffsetA * ScaleB * ScaleC + OffsetB * ScaleC + OffsetC

		OutScale = ScaleA * ScaleB * ScaleC;
		OutOffset = OffsetA * ScaleB * ScaleC + OffsetB * ScaleC + OffsetC;
	}

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	// See https://www.iquilezles.org/www/articles/smin/smin.htm
	// Unlike SmoothMin this is order-independent
	FORCEINLINE float ExponentialSmoothMin(const float DistanceA, const float DistanceB, const float Smoothness)
	{
		ensureVoxelSlow(Smoothness > 0);
		const float H = FMath::Exp(-DistanceA / Smoothness) + FMath::Exp(-DistanceB / Smoothness);
		return -FMath::Loge(H) * Smoothness;
	}

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	VOXELCORE_API void JumpFlood(
		const FIntVector& Size,
		TVoxelArrayView<float> Distances,
		TVoxelArray<float>& OutClosestX,
		TVoxelArray<float>& OutClosestY,
		TVoxelArray<float>& OutClosestZ);

	VOXELCORE_API void JumpFlood(
		const FIntVector& Size,
		TVoxelArrayView<float> Distances);
}