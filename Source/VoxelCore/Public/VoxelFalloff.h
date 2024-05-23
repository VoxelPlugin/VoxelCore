// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelFalloff.generated.h"

UENUM(BlueprintType)
enum class EVoxelFalloff : uint8
{
	None UMETA(ToolTip = "No falloff", Icon = "Icons.Denied"),
	Linear UMETA(ToolTip = "Sharp, linear falloff", Icon = "LandscapeEditor.CircleBrush_Linear"),
	Smooth UMETA(ToolTip = "Smooth falloff", Icon = "LandscapeEditor.CircleBrush_Smooth"),
	Spherical UMETA(ToolTip = "Spherical falloff, smooth at the center and sharp at the edge", Icon = "LandscapeEditor.CircleBrush_Spherical"),
	Tip UMETA(ToolTip = "Tip falloff, sharp at the center and smooth at the edge", Icon = "LandscapeEditor.CircleBrush_Tip")
};

struct FVoxelFalloff
{
public:
	FORCEINLINE static float LinearFalloff(
		const float Distance,
		const float Radius,
		const float Falloff)
	{
		return Distance <= Radius
			? 1.0f
			: Radius + Falloff <= Distance
			? 0.f
			: 1.0f - (Distance - Radius) / Falloff;
	}
	FORCEINLINE static float SmoothFalloff(
		const float Distance,
		const float Radius,
		const float Falloff)
	{
		const float X = LinearFalloff(Distance, Radius, Falloff);
		return FMath::SmoothStep(0.f, 1.f, X);
	}
	FORCEINLINE static float SphericalFalloff(
		const float Distance,
		const float Radius,
		const float Falloff)
	{
		return Distance <= Radius
			? 1.0f
			: Radius + Falloff <= Distance
			? 0.f
			: FMath::Sqrt(1.0f - FMath::Square((Distance - Radius) / Falloff));
	}
	FORCEINLINE static float TipFalloff(
		const float Distance,
		const float Radius,
		const float Falloff)
	{
		return Distance <= Radius
			? 1.0f
			: Radius + Falloff <= Distance
			? 0.f
			: 1.0f - FMath::Sqrt(1.0f - FMath::Square((Falloff + Radius - Distance) / Falloff));
	}

public:
	// Falloff: between 0 and 1
	FORCEINLINE static float GetFalloff(
		const EVoxelFalloff FalloffType,
		const float Distance,
		const float Radius,
		float Falloff)
	{
		Falloff = FMath::Clamp(Falloff, 0.f, 1.f);

		if (Falloff == 0.f ||
			FalloffType == EVoxelFalloff::None)
		{
			return Distance <= Radius ? 1.f : 0.f;
		}

		const float RelativeRadius = Radius * (1.f - Falloff);
		const float RelativeFalloff = Radius * Falloff;
		switch (FalloffType)
		{
		default: VOXEL_ASSUME(false);
		case EVoxelFalloff::Linear: return LinearFalloff(Distance, RelativeRadius, RelativeFalloff);
		case EVoxelFalloff::Smooth: return SmoothFalloff(Distance, RelativeRadius, RelativeFalloff);
		case EVoxelFalloff::Spherical: return SphericalFalloff(Distance, RelativeRadius, RelativeFalloff);
		case EVoxelFalloff::Tip: return TipFalloff(Distance, RelativeRadius, RelativeFalloff);
		}
	}
};