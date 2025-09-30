// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "StaticMeshResources.h"

namespace Voxel::Nanite
{
	class FCluster;
	struct FEncodingSettings;
}

struct VOXELCORE_API FVoxelNaniteBuilder
{
public:
	// Triangle list
	struct FMesh
	{
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
	static constexpr int32 NormalBits = 8;

	TUniquePtr<FStaticMeshRenderData> CreateRenderData(TVoxelArray<int32>& OutVertexOffsets);
	UStaticMesh* CreateStaticMesh();

public:
	static void ApplyRenderData(
		UStaticMesh& StaticMesh,
		TUniquePtr<FStaticMeshRenderData> RenderData);

	static UStaticMesh* CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData);

private:
	struct FBuildData
	{
		Nanite::FResources& Resources;
		const Voxel::Nanite::FEncodingSettings& EncodingSettings;
		TVoxelArray<TVoxelArray<Voxel::Nanite::FCluster>>& Pages;
		TVoxelChunkedArray<uint8>& RootData;
		int32 NumClusters;
		TVoxelArray<int32>& OutVertexOffsets;
		const FVoxelBox& Bounds;
	};

	bool Build(FBuildData& BuildData);
	TVoxelArray<Voxel::Nanite::FCluster> CreateClusters() const;
	TVoxelArray<TVoxelArray<Voxel::Nanite::FCluster>> CreatePages(
		TVoxelArray<Voxel::Nanite::FCluster>& Clusters,
		const Voxel::Nanite::FEncodingSettings& EncodingSettings) const;
};