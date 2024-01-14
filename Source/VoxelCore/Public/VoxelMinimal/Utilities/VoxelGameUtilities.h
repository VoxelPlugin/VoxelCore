// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class FViewport;

struct VOXELCORE_API FVoxelGameUtilities
{
public:
	static FViewport* GetViewport(const UWorld* World);

	static bool GetCameraView(const UWorld* World, FVector& OutPosition, FRotator& OutRotation, float& OutFOV);
	static bool GetCameraView(const UWorld* World, FVector& OutPosition)
	{
		FRotator Rotation;
		float FOV;
		return GetCameraView(World, OutPosition, Rotation, FOV);
	}

public:
#if WITH_EDITOR
	static bool IsActorSelected_AnyThread(FObjectKey Actor);
#endif

	static void CopyBodyInstance(FBodyInstance& Dest, const FBodyInstance& Source);

public:
	static void DrawLine(
		FObjectKey World,
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color = FLinearColor::Red,
		float Thickness = 10.f,
		float LifeTime = -1.f);

	static void DrawBox(
		FObjectKey World,
		const FVoxelBox& Box,
		const FMatrix& Transform = FMatrix::Identity,
		const FLinearColor& Color = FLinearColor::Red,
		float Thickness = 10.f,
		float LifeTime = -1.f);
};