// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNaniteBuilder.h"
#include "VoxelNanite.h"
#include "Engine/StaticMesh.h"
#include "Rendering/NaniteResources.h"

TUniquePtr<FStaticMeshRenderData> FVoxelNaniteBuilder::CreateRenderData(TVoxelArray<TVoxelArray<int32>>* OutPageToClusterToVertexOffset)
{
	VOXEL_FUNCTION_COUNTER();
	check(Mesh.Positions.Num() == Mesh.Normals.Num());
	check(Mesh.Positions.Num() % 3 == 0);

	if (!ensure(Mesh.Positions.Num() > 0))
	{
		return nullptr;
	}

	using namespace Voxel::Nanite;

	const FVoxelBox Bounds = FVoxelBox::FromPositions(Mesh.Positions);

	Nanite::FResources Resources;

	TVoxelArray<FCluster> AllClusters;
	for (int32 TriangleIndex = 0; TriangleIndex < Mesh.Positions.Num() / 3; TriangleIndex++)
	{
		if (AllClusters.Num() == 0 ||
			AllClusters.Last().NumTriangles() == NANITE_MAX_CLUSTER_TRIANGLES ||
			AllClusters.Last().Positions.Num() + 3 > NANITE_MAX_CLUSTER_VERTICES)
		{
			FCluster& Cluster = AllClusters.Emplace_GetRef();
			Cluster.TextureCoordinates.SetNum(Mesh.TextureCoordinates.Num());

			for (TVoxelArray<FVector2f>& TextureCoordinate : Cluster.TextureCoordinates)
			{
				TextureCoordinate.Reserve(128);
			}
		}

		FCluster& Cluster = AllClusters.Last();

		const int32 IndexA = 3 * TriangleIndex + 0;
		const int32 IndexB = 3 * TriangleIndex + 1;
		const int32 IndexC = 3 * TriangleIndex + 2;

		Cluster.Positions.Add(Mesh.Positions[IndexA]);
		Cluster.Positions.Add(Mesh.Positions[IndexB]);
		Cluster.Positions.Add(Mesh.Positions[IndexC]);

		Cluster.Normals.Add(Mesh.Normals[IndexA]);
		Cluster.Normals.Add(Mesh.Normals[IndexB]);
		Cluster.Normals.Add(Mesh.Normals[IndexC]);

		if (Mesh.Colors.Num() > 0)
		{
			Cluster.Colors.Add(Mesh.Colors[IndexA]);
			Cluster.Colors.Add(Mesh.Colors[IndexB]);
			Cluster.Colors.Add(Mesh.Colors[IndexC]);
		}

		for (int32 UVIndex = 0; UVIndex < Cluster.TextureCoordinates.Num(); UVIndex++)
		{
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexA]);
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexB]);
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexC]);
		}
	}

	const int32 TreeDepth = FMath::CeilToInt(FMath::LogX(4.f, AllClusters.Num()));
	ensure(TreeDepth < NANITE_MAX_CLUSTER_HIERARCHY_DEPTH);

	const auto MakeHierarchyNode = [&]
	{
		Nanite::FPackedHierarchyNode HierarchyNode;

		for (int32 Index = 0; Index < 4; Index++)
		{
			HierarchyNode.LODBounds[Index] = FVector4f(
				Bounds.GetCenter().X,
				Bounds.GetCenter().Y,
				Bounds.GetCenter().Z,
				Bounds.Size().Length());

			HierarchyNode.Misc0[Index].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);
			HierarchyNode.Misc0[Index].BoxBoundsCenter = FVector3f(Bounds.GetCenter());
			HierarchyNode.Misc1[Index].BoxBoundsExtent = FVector3f(Bounds.GetExtent());
			HierarchyNode.Misc1[Index].ChildStartReference = 0xFFFFFFFF;
			HierarchyNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize = 0;
		}

		return HierarchyNode;
	};

	TVoxelArray<int32> LeafNodes;
	for (int32 Depth = 0; Depth <= TreeDepth; Depth++)
	{
		if (Depth == 0)
		{
			Resources.HierarchyNodes.Add(MakeHierarchyNode());
			LeafNodes.Add(0);
			continue;
		}

		Resources.HierarchyNodes.Reserve(Resources.HierarchyNodes.Num() + 4 * LeafNodes.Num());

		TVoxelArray<int32> NewLeafNodes;
		for (const int32 ParentIndex : LeafNodes)
		{
			Nanite::FPackedHierarchyNode& ParentNode = Resources.HierarchyNodes[ParentIndex];
			for (int32 Index = 0; Index < 4; Index++)
			{
				const int32 ChildIndex = Resources.HierarchyNodes.Add(MakeHierarchyNode());

				ensure(ParentNode.Misc1[Index].ChildStartReference == 0xFFFFFFFF);
				ensure(ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize == 0);

				ParentNode.Misc1[Index].ChildStartReference = ChildIndex;
				ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;

				NewLeafNodes.Add(ChildIndex);
			}
		}

		LeafNodes = MoveTemp(NewLeafNodes);
	}
	check(AllClusters.Num() <= LeafNodes.Num());

	for (int32 ClusterIndex = 0; ClusterIndex < AllClusters.Num(); ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode& HierarchyNode = Resources.HierarchyNodes[LeafNodes[ClusterIndex]];

		ensure(HierarchyNode.Misc1[0].ChildStartReference == 0xFFFFFFFF);
		ensure(HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize == 0);

		HierarchyNode.Misc1[0].ChildStartReference = Resources.HierarchyNodes.Num() + ClusterIndex;
		HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;
	}

	FEncodingSettings EncodingSettings;
	EncodingSettings.PositionPrecision = PositionPrecision;
	checkStatic(FEncodingSettings::NormalBits == NormalBits);

	TVoxelArray<TVoxelArray<FCluster>> Pages;
	{
		int32 ClusterIndex = 0;
		while (ClusterIndex < AllClusters.Num())
		{
			TVoxelArray<FCluster>& Clusters = Pages.Emplace_GetRef();
			int32 GpuSize = 0;

			while (
				ClusterIndex < AllClusters.Num() &&
				Clusters.Num() < NANITE_ROOT_PAGE_MAX_CLUSTERS)
			{
				FCluster& Cluster = AllClusters[ClusterIndex];

				const int32 ClusterGpuSize = Cluster.GetEncodingInfo(EncodingSettings).GpuSizes.GetTotal();
				if (GpuSize + ClusterGpuSize > NANITE_ROOT_PAGE_GPU_SIZE)
				{
					break;
				}

				Clusters.Add(MoveTemp(Cluster));
				GpuSize += ClusterGpuSize;
				ClusterIndex++;
			}

			ensure(GpuSize <= NANITE_ROOT_PAGE_GPU_SIZE);
		}
		check(ClusterIndex == AllClusters.Num());
	}

	if (OutPageToClusterToVertexOffset)
	{
		int32 VertexOffset = 0;
		for (const TVoxelArray<FCluster>& Page : Pages)
		{
			TVoxelArray<int32>& ClusterToVertexOffset = OutPageToClusterToVertexOffset->Emplace_GetRef();

			for (const FCluster& Cluster : Page)
			{
				ClusterToVertexOffset.Add(VertexOffset);
				VertexOffset += Cluster.NumVertices();
			}
		}
	}

	TVoxelChunkedArray<uint8> RootData;

	int32 ClusterIndexOffset = 0;
	for (int32 PageIndex = 0; PageIndex < Pages.Num(); PageIndex++)
	{
		TVoxelArray<FCluster>& Clusters = Pages[PageIndex];
		ON_SCOPE_EXIT
		{
			ClusterIndexOffset += Clusters.Num();
		};

		Nanite::FPageStreamingState PageStreamingState{};
		PageStreamingState.BulkOffset = RootData.Num();

		Nanite::FFixupChunk FixupChunk;
		FixupChunk.Header.Magic = NANITE_FIXUP_MAGIC;
		FixupChunk.Header.NumClusters = Clusters.Num();
		FixupChunk.Header.NumHierachyFixups = Clusters.Num();
		FixupChunk.Header.NumClusterFixups = Clusters.Num();

		const TVoxelArrayView<uint8> HierarchyFixupsData = MakeVoxelArrayView(FixupChunk.Data).LeftOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<uint8> ClusterFixupsData = MakeVoxelArrayView(FixupChunk.Data).RightOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<Nanite::FHierarchyFixup> HierarchyFixups = HierarchyFixupsData.ReinterpretAs<Nanite::FHierarchyFixup>();
		const TVoxelArrayView<Nanite::FClusterFixup> ClusterFixups = ClusterFixupsData.ReinterpretAs<Nanite::FClusterFixup>().LeftOf(Clusters.Num());

		for (int32 Index = 0; Index < Clusters.Num(); Index++)
		{
			HierarchyFixups[Index] = Nanite::FHierarchyFixup(
				PageIndex,
				Resources.HierarchyNodes.Num() + ClusterIndexOffset + Index,
				0,
				Index,
				0,
				0);

			ClusterFixups[Index] = Nanite::FClusterFixup(
				PageIndex,
				Index,
				0,
				0);
		}

		RootData.Append(MakeByteVoxelArrayView(FixupChunk).LeftOf(FixupChunk.GetSize()));

		const int32 PageStartIndex = RootData.Num();

		CreatePageData(
			Clusters,
			EncodingSettings,
			RootData);

		PageStreamingState.BulkSize = RootData.Num() - PageStreamingState.BulkOffset;
		PageStreamingState.PageSize = RootData.Num() - PageStartIndex;
		PageStreamingState.MaxHierarchyDepth = NANITE_MAX_CLUSTER_HIERARCHY_DEPTH;
		Resources.PageStreamingStates.Add(PageStreamingState);
	}

	for (int32 ClusterIndex = 0; ClusterIndex < AllClusters.Num(); ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode PackedHierarchyNode;
		FMemory::Memzero(PackedHierarchyNode);

		PackedHierarchyNode.Misc0[0].BoxBoundsCenter = FVector3f(Bounds.GetCenter());
		PackedHierarchyNode.Misc0[0].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);

		PackedHierarchyNode.Misc1[0].BoxBoundsExtent = FVector3f(Bounds.GetExtent());
		PackedHierarchyNode.Misc1[0].ChildStartReference = 0xFFFFFFFFu;

		const int32 PageIndexStart = 0;
		const int32 PageIndexNum = 0;
		const int32 GroupPartSize = 1;
		PackedHierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize =
			(PageIndexStart << (NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS)) |
			(PageIndexNum << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) |
			GroupPartSize;

		Resources.HierarchyNodes.Add(PackedHierarchyNode);
	}

	Resources.RootData = RootData.Array<FDefaultAllocator>();
	Resources.PositionPrecision = -1;
	Resources.NormalPrecision = -1;
	Resources.NumInputTriangles = 0;
	Resources.NumInputVertices = Mesh.Positions.Num();
	Resources.NumInputMeshes = 1;
	Resources.NumInputTexCoords = Mesh.TextureCoordinates.Num();
	Resources.NumClusters = AllClusters.Num();
	Resources.NumRootPages = Pages.Num();
	Resources.HierarchyRootOffsets.Add(0);

	TUniquePtr<FStaticMeshRenderData> RenderData = MakeUnique<FStaticMeshRenderData>();
	RenderData->Bounds = Bounds.ToFBox();
	RenderData->NaniteResourcesPtr = MakePimpl<Nanite::FResources>(MoveTemp(Resources));

	FStaticMeshLODResources* LODResource = new FStaticMeshLODResources();
	LODResource->bBuffersInlined = true;
	LODResource->Sections.Emplace();
	RenderData->LODResources.Add(LODResource);

	RenderData->LODVertexFactories.Emplace_GetRef(GMaxRHIFeatureLevel);

	return RenderData;
}

UStaticMesh* FVoxelNaniteBuilder::CreateStaticMesh()
{
	VOXEL_FUNCTION_COUNTER();

	return CreateStaticMesh(CreateRenderData());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelNaniteBuilder::ApplyRenderData(UStaticMesh& StaticMesh, TUniquePtr<FStaticMeshRenderData> RenderData)
{
	VOXEL_FUNCTION_COUNTER();

	StaticMesh.ReleaseResources();

	StaticMesh.SetStaticMaterials({ FStaticMaterial() });
	StaticMesh.SetRenderData(MoveTemp(RenderData));
	StaticMesh.CalculateExtendedBounds();
#if WITH_EDITOR
	StaticMesh.NaniteSettings.bEnabled = true;
#endif

	// Not supported, among other issues FSceneProxy::FSceneProxy crashes because GetNumVertices is always 0
	StaticMesh.bSupportRayTracing = false;

	StaticMesh.InitResources();
}

UStaticMesh* FVoxelNaniteBuilder::CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>();
	ApplyRenderData(*StaticMesh, MoveTemp(RenderData));
	return StaticMesh;
}