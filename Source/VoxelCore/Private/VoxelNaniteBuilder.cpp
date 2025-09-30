// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNaniteBuilder.h"
#include "VoxelNanite.h"
#include "Engine/StaticMesh.h"
#include "Rendering/NaniteResources.h"

#if VOXEL_ENGINE_VERSION >= 507
#include "Nanite/NaniteFixupChunk.h"
#endif

TUniquePtr<FStaticMeshRenderData> FVoxelNaniteBuilder::CreateRenderData(TVoxelArray<int32>& OutVertexOffsets)
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

	TVoxelArray<FCluster> AllClusters = CreateClusters();

	FEncodingSettings EncodingSettings;
	EncodingSettings.PositionPrecision = PositionPrecision;
	checkStatic(FEncodingSettings::NormalBits == NormalBits);

	TVoxelArray<TVoxelArray<FCluster>> Pages = CreatePages(AllClusters, EncodingSettings);

	TVoxelChunkedArray<uint8> RootData;

	FBuildData BuildData{
		Resources,
		EncodingSettings,
		Pages,
		RootData,
		AllClusters.Num(),
		OutVertexOffsets,
		Bounds
	};
	if (!Build(BuildData))
	{
		return nullptr;
	}

	Resources.RootData = RootData.Array();
	Resources.PositionPrecision = -1;
	Resources.NormalPrecision = -1;
	Resources.NumInputTriangles = 0;
	Resources.NumInputVertices = Mesh.Positions.Num();
#if VOXEL_ENGINE_VERSION < 506
	Resources.NumInputMeshes = 1;
	Resources.NumInputTexCoords = Mesh.TextureCoordinates.Num();
#endif
	Resources.NumClusters = AllClusters.Num();
	Resources.NumRootPages = Pages.Num();
	Resources.HierarchyRootOffsets.Add(0);
#if VOXEL_ENGINE_VERSION >= 507
	Resources.MeshBounds = Bounds.ToFBox3f();
#endif

	TUniquePtr<FStaticMeshRenderData> RenderData = MakeUnique<FStaticMeshRenderData>();
	RenderData->Bounds = Bounds.ToFBox();
	RenderData->NumInlinedLODs = 1;
	RenderData->NaniteResourcesPtr = MakePimpl<Nanite::FResources>(MoveTemp(Resources));

	FStaticMeshLODResources* LODResource = new FStaticMeshLODResources();
	LODResource->bBuffersInlined = true;
	LODResource->Sections.Emplace();

	// Ensure UStaticMesh::HasValidRenderData returns true
	// Use MAX_flt to try to not have the vertex picked by vertex snapping
	const TVoxelArray<FVector3f> DummyPositions = { FVector3f(MAX_flt) };

	LODResource->VertexBuffers.StaticMeshVertexBuffer.Init(DummyPositions.Num(), 1);
	LODResource->VertexBuffers.PositionVertexBuffer.Init(DummyPositions);
	LODResource->VertexBuffers.ColorVertexBuffer.Init(DummyPositions.Num());

	// Ensure FStaticMeshRenderData::GetFirstValidLODIdx doesn't return -1
	LODResource->BuffersSize = 1;

	RenderData->LODResources.Add(LODResource);

	RenderData->LODVertexFactories.Add(FStaticMeshVertexFactories(GMaxRHIFeatureLevel));

	return RenderData;
}

UStaticMesh* FVoxelNaniteBuilder::CreateStaticMesh()
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<int32> VertexOffsets;
	return CreateStaticMesh(CreateRenderData(VertexOffsets));
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
	StaticMesh.UE_507_SWITCH(NaniteSettings, GetNaniteSettings()).bEnabled = true;
#endif

	// Not supported, among other issues FSceneProxy::FSceneProxy crashes because GetNumVertices is always 0
	StaticMesh.bSupportRayTracing = false;

	VOXEL_SCOPE_COUNTER("UStaticMesh::InitResources");
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelNaniteBuilder::Build(FBuildData& BuildData)
#if VOXEL_ENGINE_VERSION >= 507
{
	using namespace Voxel::Nanite;

	const int32 TreeDepth = FMath::FloorToInt32(FMath::LogX(4.f, FMath::Max(BuildData.NumClusters - 1, 1)));
	ensure(TreeDepth < NANITE_MAX_CLUSTER_HIERARCHY_DEPTH);

	const auto MakeHierarchyNode = [&]
	{
		Nanite::FPackedHierarchyNode HierarchyNode;

		for (int32 Index = 0; Index < 4; Index++)
		{
			HierarchyNode.LODBounds[Index] = FVector4f(
				BuildData.Bounds.GetCenter().X,
				BuildData.Bounds.GetCenter().Y,
				BuildData.Bounds.GetCenter().Z,
				BuildData.Bounds.Size().Length());

			HierarchyNode.Misc0[Index].MinLODError_MaxParentLODError = FFloat16(1e10f).Encoded | (FFloat16(-1).Encoded << 16);
			HierarchyNode.Misc0[Index].BoxBoundsCenter = FVector3f(BuildData.Bounds.GetCenter());
			HierarchyNode.Misc1[Index].BoxBoundsExtent = FVector3f(BuildData.Bounds.GetExtent());
			HierarchyNode.Misc1[Index].ChildStartReference = 0xFFFFFFFF;
			HierarchyNode.Misc2[Index].ResourcePageRangeKey = NANITE_PAGE_RANGE_KEY_EMPTY_RANGE;
			HierarchyNode.Misc2[Index].GroupPartSize_AssemblyPartIndex = 0;
		}

		return HierarchyNode;
	};

	TVoxelArray<int32> LeafNodes;
	for (int32 Depth = 0; Depth <= TreeDepth; Depth++)
	{
		if (Depth == 0)
		{
			BuildData.Resources.HierarchyNodes.Add(MakeHierarchyNode());
			LeafNodes.Add(0);
			continue;
		}

		BuildData.Resources.HierarchyNodes.Reserve(BuildData.Resources.HierarchyNodes.Num() + 4 * LeafNodes.Num());

		TVoxelArray<int32> NewLeafNodes;
		for (const int32 ParentIndex : LeafNodes)
		{
			Nanite::FPackedHierarchyNode& ParentNode = BuildData.Resources.HierarchyNodes[ParentIndex];
			for (int32 Index = 0; Index < 4; Index++)
			{
				const int32 ChildIndex = BuildData.Resources.HierarchyNodes.Add(MakeHierarchyNode());

				ensure(ParentNode.Misc1[Index].ChildStartReference == 0xFFFFFFFF);
				ensure(
					ParentNode.Misc2[Index].ResourcePageRangeKey == NANITE_PAGE_RANGE_KEY_EMPTY_RANGE &&
					ParentNode.Misc2[Index].GroupPartSize_AssemblyPartIndex == 0);

				ParentNode.Misc1[Index].ChildStartReference = ChildIndex;

				ParentNode.Misc2[Index].ResourcePageRangeKey = 0xFFFFFFFFu;
				ParentNode.Misc2[Index].GroupPartSize_AssemblyPartIndex =
					(0xFFFFFFFFu & NANITE_HIERARCHY_MAX_ASSEMBLY_TRANSFORMS) |
					(0 << NANITE_HIERARCHY_ASSEMBLY_TRANSFORM_INDEX_BITS);

				NewLeafNodes.Add(ChildIndex);
			}
		}

		LeafNodes = MoveTemp(NewLeafNodes);
	}
	check(BuildData.NumClusters >= LeafNodes.Num());

	struct FClusterHierarchyNode
	{
		int32 HierarchyNodeIndex;
		int32 NodePartIndex;
	};
	TVoxelArray<FClusterHierarchyNode> ClusterIndexToLeafNode;
	ClusterIndexToLeafNode.Reserve(LeafNodes.Num() * 4);
	for (const int32 LeafNodeIndex : LeafNodes)
	{
		ClusterIndexToLeafNode.Add({ LeafNodeIndex, 0 });
		ClusterIndexToLeafNode.Add({ LeafNodeIndex, 1 });
		ClusterIndexToLeafNode.Add({ LeafNodeIndex, 2 });
		ClusterIndexToLeafNode.Add({ LeafNodeIndex, 3 });
	}
	if (!ensure(BuildData.NumClusters <= ClusterIndexToLeafNode.Num()))
	{
		return false;
	}

	int32 VertexOffset = 0;
	int32 ClusterIndexOffset = 0;
	for (int32 PageIndex = 0; PageIndex < BuildData.Pages.Num(); PageIndex++)
	{
		TVoxelArray<FCluster>& PageClusters = BuildData.Pages[PageIndex];
		ON_SCOPE_EXIT
		{
			ClusterIndexOffset += PageClusters.Num();
		};

		for (int32 ClusterIndex = 0; ClusterIndex < PageClusters.Num(); ClusterIndex++)
		{
			FCluster& Cluster = PageClusters[ClusterIndex];

			const FClusterHierarchyNode& LeafNodeData = ClusterIndexToLeafNode[ClusterIndexOffset + ClusterIndex];
			Nanite::FPackedHierarchyNode& HierarchyNode = BuildData.Resources.HierarchyNodes[LeafNodeData.HierarchyNodeIndex];

			const FVoxelBox& ClusterBounds = Cluster.GetBounds();
			HierarchyNode.LODBounds[LeafNodeData.NodePartIndex] = FVector4f(FVector3f(ClusterBounds.Size()), ClusterBounds.Size().Length());

			ensure(HierarchyNode.Misc1[LeafNodeData.NodePartIndex].ChildStartReference == 0xFFFFFFFF);
			ensure(
				HierarchyNode.Misc2[LeafNodeData.NodePartIndex].ResourcePageRangeKey == NANITE_PAGE_RANGE_KEY_EMPTY_RANGE &&
				HierarchyNode.Misc2[LeafNodeData.NodePartIndex].GroupPartSize_AssemblyPartIndex == 0);

			static constexpr uint32 AssemblyPartIndex = 0xFFFFFFFFu;
			static constexpr int32 GroupPartSize = 1;
			HierarchyNode.Misc2[LeafNodeData.NodePartIndex].ResourcePageRangeKey = Nanite::FPageRangeKey(PageIndex, 1, false, false).Value;
			HierarchyNode.Misc2[LeafNodeData.NodePartIndex].GroupPartSize_AssemblyPartIndex =
				(AssemblyPartIndex & NANITE_HIERARCHY_MAX_ASSEMBLY_TRANSFORMS) |
				(GroupPartSize << NANITE_HIERARCHY_ASSEMBLY_TRANSFORM_INDEX_BITS);
		}

		Nanite::FPageStreamingState PageStreamingState{};
		PageStreamingState.BulkOffset = BuildData.RootData.Num();

		TArray<uint8> FixupChunkData;

		// Fixup
		{
			const uint32 NumGroups = 1;
			const uint32 NumParts = PageClusters.Num();
			const uint32 NumHierarchyLocations = PageClusters.Num();

			const uint32 FixupChunkSize = Nanite::FFixupChunk::GetSize(
				NumGroups,
				NumParts,
				0,
				NumHierarchyLocations,
				0,
				0);
			FixupChunkData.Init(0x00, FixupChunkSize);

			Nanite::FFixupChunk& FixupChunk = *reinterpret_cast<Nanite::FFixupChunk*>(FixupChunkData.GetData());
			FixupChunk.Header.Magic = NANITE_FIXUP_MAGIC;
			FixupChunk.Header.NumGroupFixups = IntCastChecked<uint16>(NumGroups);
			FixupChunk.Header.NumPartFixups = IntCastChecked<uint16>(NumParts);
			FixupChunk.Header.NumClusters = IntCastChecked<uint16>(PageClusters.Num());
			FixupChunk.Header.NumReconsiderPages = IntCastChecked<uint16>(0);
			FixupChunk.Header.NumParentFixups = 0;
			FixupChunk.Header.NumHierarchyLocations = NumHierarchyLocations;
			FixupChunk.Header.NumClusterIndices = 0;

			// Page Fixup
			{
				Nanite::FFixupChunk::FGroupFixup& GroupFixup = FixupChunk.GetGroupFixup(0);
				GroupFixup.PageDependencies = Nanite::FPageRangeKey(PageIndex, 1, false, false);
				GroupFixup.Flags = 0u;
				GroupFixup.FirstPartFixup = IntCastChecked<uint16>(0);
				GroupFixup.NumPartFixups = IntCastChecked<uint16>(NumParts);
				GroupFixup.FirstParentFixup = IntCastChecked<uint16>(0);
				GroupFixup.NumParentFixups = IntCastChecked<uint16>(0);
			}

			for (int32 ClusterIndex = 0; ClusterIndex < PageClusters.Num(); ClusterIndex++)
			{
				// Part Fixup
				Nanite::FFixupChunk::FPartFixup& PartFixup = FixupChunk.GetPartFixup(ClusterIndex);
				PartFixup.PageIndex = IntCastChecked<uint16>(PageIndex);
				PartFixup.StartClusterIndex = IntCastChecked<uint8>(ClusterIndex);
				PartFixup.LeafCounter = IntCastChecked<uint8>(0);
				PartFixup.FirstHierarchyLocation = ClusterIndex;
				PartFixup.NumHierarchyLocations = IntCastChecked<uint16>(1);

				// Hierarchy Location Fixup
				const FClusterHierarchyNode& LeafNodeData = ClusterIndexToLeafNode[ClusterIndexOffset + ClusterIndex];
				FixupChunk.GetHierarchyLocation(ClusterIndex) = Nanite::FFixupChunk::FHierarchyLocation(LeafNodeData.HierarchyNodeIndex, LeafNodeData.NodePartIndex);
			}
		}

		BuildData.RootData.Append(FixupChunkData);

		const int32 PageStartIndex = BuildData.RootData.Num();

		BuildData.OutVertexOffsets.Add(VertexOffset);

		CreatePageData(
			PageClusters,
			BuildData.EncodingSettings,
			BuildData.RootData,
			VertexOffset);

		PageStreamingState.DependenciesStart = 0;
		PageStreamingState.DependenciesNum = 0;
		PageStreamingState.BulkSize = BuildData.RootData.Num() - PageStreamingState.BulkOffset;
		PageStreamingState.PageSize = BuildData.RootData.Num() - PageStartIndex;
		PageStreamingState.MaxHierarchyDepth = NANITE_MAX_CLUSTER_HIERARCHY_DEPTH;
		BuildData.Resources.PageStreamingStates.Add(PageStreamingState);
	}
	return true;
}
#else
{
	using namespace Voxel::Nanite;

	const int32 TreeDepth = FMath::CeilToInt(FMath::LogX(4.f, BuildData.NumClusters));
	ensure(TreeDepth < NANITE_MAX_CLUSTER_HIERARCHY_DEPTH);

	const auto MakeHierarchyNode = [&]
	{
		Nanite::FPackedHierarchyNode HierarchyNode;

		for (int32 Index = 0; Index < 4; Index++)
		{
			HierarchyNode.LODBounds[Index] = FVector4f(
				BuildData.Bounds.GetCenter().X,
				BuildData.Bounds.GetCenter().Y,
				BuildData.Bounds.GetCenter().Z,
				BuildData.Bounds.Size().Length());

			HierarchyNode.Misc0[Index].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);
			HierarchyNode.Misc0[Index].BoxBoundsCenter = FVector3f(BuildData.Bounds.GetCenter());
			HierarchyNode.Misc1[Index].BoxBoundsExtent = FVector3f(BuildData.Bounds.GetExtent());
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
			BuildData.Resources.HierarchyNodes.Add(MakeHierarchyNode());
			LeafNodes.Add(0);
			continue;
		}

		BuildData.Resources.HierarchyNodes.Reserve(BuildData.Resources.HierarchyNodes.Num() + 4 * LeafNodes.Num());

		TVoxelArray<int32> NewLeafNodes;
		for (const int32 ParentIndex : LeafNodes)
		{
			Nanite::FPackedHierarchyNode& ParentNode = BuildData.Resources.HierarchyNodes[ParentIndex];
			for (int32 Index = 0; Index < 4; Index++)
			{
				const int32 ChildIndex = BuildData.Resources.HierarchyNodes.Add(MakeHierarchyNode());

				ensure(ParentNode.Misc1[Index].ChildStartReference == 0xFFFFFFFF);
				ensure(ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize == 0);

				ParentNode.Misc1[Index].ChildStartReference = ChildIndex;
				ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;

				NewLeafNodes.Add(ChildIndex);
			}
		}

		LeafNodes = MoveTemp(NewLeafNodes);
	}
	check(BuildData.NumClusters <= LeafNodes.Num());

	for (int32 ClusterIndex = 0; ClusterIndex < BuildData.NumClusters; ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode& HierarchyNode = BuildData.Resources.HierarchyNodes[LeafNodes[ClusterIndex]];

		ensure(HierarchyNode.Misc1[0].ChildStartReference == 0xFFFFFFFF);
		ensure(HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize == 0);

		HierarchyNode.Misc1[0].ChildStartReference = BuildData.Resources.HierarchyNodes.Num() + ClusterIndex;
		HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;
	}

	int32 VertexOffset = 0;
	int32 ClusterIndexOffset = 0;
	for (int32 PageIndex = 0; PageIndex < BuildData.Pages.Num(); PageIndex++)
	{
		TVoxelArray<FCluster>& Clusters = BuildData.Pages[PageIndex];
		ON_SCOPE_EXIT
		{
			ClusterIndexOffset += Clusters.Num();
		};

		Nanite::FPageStreamingState PageStreamingState{};
		PageStreamingState.BulkOffset = BuildData.RootData.Num();

#if VOXEL_ENGINE_VERSION < 506
		Nanite::FFixupChunk FixupChunk;
		FixupChunk.Header.Magic = NANITE_FIXUP_MAGIC;
		FixupChunk.Header.NumClusters = Clusters.Num();
		FixupChunk.Header.UE_506_SWITCH(NumHierachyFixups, NumHierarchyFixups) = Clusters.Num();
		FixupChunk.Header.NumClusterFixups = Clusters.Num();
#else
		Nanite::FFixupChunkBuffer FixupChunkBuffer;
		Nanite::FFixupChunk& FixupChunk = FixupChunkBuffer.Add_GetRef(
			Clusters.Num(),
			Clusters.Num(),
			Clusters.Num());
#endif

#if VOXEL_ENGINE_VERSION < 506
		const TVoxelArrayView<uint8> HierarchyFixupsData = MakeVoxelArrayView(FixupChunk.Data).LeftOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<uint8> ClusterFixupsData = MakeVoxelArrayView(FixupChunk.Data).RightOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<Nanite::FHierarchyFixup> HierarchyFixups = HierarchyFixupsData.ReinterpretAs<Nanite::FHierarchyFixup>();
		const TVoxelArrayView<Nanite::FClusterFixup> ClusterFixups = ClusterFixupsData.ReinterpretAs<Nanite::FClusterFixup>().LeftOf(Clusters.Num());
#endif

		for (int32 Index = 0; Index < Clusters.Num(); Index++)
		{
			UE_506_SWITCH(HierarchyFixups[Index], FixupChunk.GetHierarchyFixup(Index)) = Nanite::FHierarchyFixup(
				PageIndex,
				BuildData.Resources.HierarchyNodes.Num() + ClusterIndexOffset + Index,
				0,
				Index,
				0,
				0);

			UE_506_SWITCH(ClusterFixups[Index], FixupChunk.GetClusterFixup(Index)) = Nanite::FClusterFixup(
				PageIndex,
				Index,
				0,
				0);
		}

#if VOXEL_ENGINE_VERSION < 506
		BuildData.RootData.Append(MakeByteVoxelArrayView(FixupChunk).LeftOf(FixupChunk.GetSize()));
#else
		BuildData.RootData.Append(TConstVoxelArrayView<uint8>(
			reinterpret_cast<uint8*>(&FixupChunk),
			FixupChunk.GetSize()));
#endif

		const int32 PageStartIndex = BuildData.RootData.Num();

		BuildData.OutVertexOffsets.Add(VertexOffset);

		CreatePageData(
			Clusters,
			BuildData.EncodingSettings,
			BuildData.RootData,
			VertexOffset);

		PageStreamingState.BulkSize = BuildData.RootData.Num() - PageStreamingState.BulkOffset;
		PageStreamingState.PageSize = BuildData.RootData.Num() - PageStartIndex;
		PageStreamingState.MaxHierarchyDepth = NANITE_MAX_CLUSTER_HIERARCHY_DEPTH;
		BuildData.Resources.PageStreamingStates.Add(PageStreamingState);
	}

	for (int32 ClusterIndex = 0; ClusterIndex < BuildData.NumClusters; ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode PackedHierarchyNode;
		FMemory::Memzero(PackedHierarchyNode);

		PackedHierarchyNode.Misc0[0].BoxBoundsCenter = FVector3f(BuildData.Bounds.GetCenter());
		PackedHierarchyNode.Misc0[0].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);

		PackedHierarchyNode.Misc1[0].BoxBoundsExtent = FVector3f(BuildData.Bounds.GetExtent());
		PackedHierarchyNode.Misc1[0].ChildStartReference = 0xFFFFFFFFu;

		const int32 PageIndexStart = 0;
		const int32 PageIndexNum = 0;
		const int32 GroupPartSize = 1;
		PackedHierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize =
			(PageIndexStart << (NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS)) |
			(PageIndexNum << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) |
			GroupPartSize;

		BuildData.Resources.HierarchyNodes.Add(PackedHierarchyNode);
	}

	return true;
}
#endif

TVoxelArray<Voxel::Nanite::FCluster> FVoxelNaniteBuilder::CreateClusters() const
{
	using namespace Voxel::Nanite;

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
	return AllClusters;
}

TVoxelArray<TVoxelArray<Voxel::Nanite::FCluster>> FVoxelNaniteBuilder::CreatePages(
	TVoxelArray<Voxel::Nanite::FCluster>& Clusters,
	const Voxel::Nanite::FEncodingSettings& EncodingSettings) const
{
	using namespace Voxel::Nanite;
	TVoxelArray<TVoxelArray<FCluster>> Pages;
	{
		int32 ClusterIndex = 0;
		while (ClusterIndex < Clusters.Num())
		{
			TVoxelArray<FCluster>& PageClusters = Pages.Emplace_GetRef();
			int32 GpuSize = 0;

			while (
				ClusterIndex < Clusters.Num() &&
				PageClusters.Num() < NANITE_ROOT_PAGE_MAX_CLUSTERS)
			{
				FCluster& Cluster = Clusters[ClusterIndex];

				const int32 ClusterGpuSize = Cluster.GetEncodingInfo(EncodingSettings).GpuSizes.GetTotal();
				if (GpuSize + ClusterGpuSize > NANITE_ROOT_PAGE_GPU_SIZE)
				{
					break;
				}

				PageClusters.Add(MoveTemp(Cluster));
				GpuSize += ClusterGpuSize;
				ClusterIndex++;
			}

			ensure(GpuSize <= NANITE_ROOT_PAGE_GPU_SIZE);
		}
		check(ClusterIndex == Clusters.Num());
	}
	return Pages;
}