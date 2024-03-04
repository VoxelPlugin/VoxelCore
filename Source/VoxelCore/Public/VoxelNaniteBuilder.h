// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "StaticMeshResources.h"

struct VOXELCORE_API FVoxelNaniteBuilder
{
public:
	struct FMesh
	{
		TConstVoxelArrayView<int32> Indices;
		TConstVoxelArrayView<FVector3f> Positions;
		TConstVoxelArrayView<FVoxelOctahedron> Normals;
		// Optional
		TConstVoxelArrayView<FColor> Colors;
		// Optional
		TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinates;
	};
	FMesh Mesh;

	// Step is 2^(-PositionPrecision)
	int32 PositionPrecision = 4;
	int32 UVBits = 8;
	static constexpr int32 NormalBits = 8;

	TUniquePtr<FStaticMeshRenderData> CreateRenderData();
	UStaticMesh* CreateStaticMesh();

public:
	static void ApplyRenderData(
		UStaticMesh& StaticMesh,
		TUniquePtr<FStaticMeshRenderData> RenderData);

	static UStaticMesh* CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData);
};