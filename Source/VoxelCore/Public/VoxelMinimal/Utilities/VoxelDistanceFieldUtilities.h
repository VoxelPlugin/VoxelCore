// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

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

	FORCEINLINE float SmoothMin(const float DistanceA, const float DistanceB, const float Smoothness)
	{
		const float H = FMath::Clamp(0.5f + 0.5f * (DistanceB - DistanceA) / Smoothness, 0.0f, 1.0f);
		return FMath::Lerp(DistanceB, DistanceA, H) - Smoothness * H * (1.0f - H);
	}
	FORCEINLINE float SmoothMax(const float DistanceA, const float DistanceB, const float Smoothness)
	{
		return -SmoothMin(-DistanceA, -DistanceB, Smoothness);
	}

	// See https://www.iquilezles.org/www/articles/smin/smin.htm
	// Unlike SmoothMin this is order-independent
	FORCEINLINE float ExponentialSmoothMin(const float DistanceA, const float DistanceB, const float Smoothness)
	{
		ensureVoxelSlow(Smoothness > 0);
		const float H = FMath::Exp(-DistanceA / Smoothness) + FMath::Exp(-DistanceB / Smoothness);
		return -FMath::Loge(H) * Smoothness;
	}
}