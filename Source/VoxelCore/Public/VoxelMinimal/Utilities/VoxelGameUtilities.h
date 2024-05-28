// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class FViewport;

namespace FVoxelUtilities
{
	VOXELCORE_API FViewport* GetViewport(const UWorld* World);

	VOXELCORE_API bool GetCameraView(const UWorld* World, FVector& OutPosition, FRotator& OutRotation, float& OutFOV);

	FORCEINLINE bool GetCameraView(const UWorld* World, FVector& OutPosition)
	{
		FRotator Rotation;
		float FOV;
		return GetCameraView(World, OutPosition, Rotation, FOV);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	VOXELCORE_API bool IsActorSelected_AnyThread(FObjectKey Actor);
#endif

	VOXELCORE_API void CopyBodyInstance(
		FBodyInstance& Dest,
		const FBodyInstance& Source);

	VOXELCORE_API bool BodyInstanceEqual(
		const FBodyInstance& A,
		const FBodyInstance& B);
}