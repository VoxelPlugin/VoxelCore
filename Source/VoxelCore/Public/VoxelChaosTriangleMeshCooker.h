// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelChaosTriangleMeshCooker
{
	static TRefCountPtr<Chaos::FTriangleMeshImplicitObject> Create(
		TConstVoxelArrayView<int32> Indices,
		TConstVoxelArrayView<FVector3f> Vertices,
		TConstVoxelArrayView<uint16> FaceMaterials);

	static int64 GetAllocatedSize(const Chaos::FTriangleMeshImplicitObject& TriangleMesh);
};