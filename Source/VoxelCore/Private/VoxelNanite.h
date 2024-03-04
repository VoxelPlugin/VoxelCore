// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "NaniteDefinitions.h"

namespace Nanite
{
	struct FPackedCluster;
}

namespace Voxel::Nanite
{
using FPackedCluster = ::Nanite::FPackedCluster;

struct FPageGPUHeader
{
	uint32 NumClusters = 0;
	uint32 Pad[3] = { 0 };
};

struct FPageDiskHeader
{
	uint32 GpuSize = 0;
	uint32 NumClusters = 0;
	uint32 NumRawFloat4s = 0;
	uint32 NumTexCoords = 0;
	uint32 NumVertexRefs = 0;
	uint32 DecodeInfoOffset = 0;
	uint32 StripBitmaskOffset = 0;
	uint32 VertexRefBitmaskOffset = 0;
};

struct FClusterDiskHeader
{
	uint32 IndexDataOffset = 0;
	uint32 PageClusterMapOffset = 0;
	uint32 VertexRefDataOffset = 0;
	uint32 PositionDataOffset = 0;
	uint32 AttributeDataOffset = 0;
	uint32 NumVertexRefs = 0;
	uint32 NumPrevRefVerticesBeforeDwords = 0;
	uint32 NumPrevNewVerticesBeforeDwords = 0;
};

struct FPageSections
{
	uint32 Cluster = 0;
	uint32 MaterialTable = 0;
	uint32 VertReuseBatchInfo = 0;
	uint32 DecodeInfo = 0;
	uint32 Index = 0;
	uint32 Position = 0;
	uint32 Attribute = 0;

	uint32 GetMaterialTableSize() const
	{
		return Align(MaterialTable, 16);
	}
	uint32 GetVertReuseBatchInfoSize() const
	{
		return Align(VertReuseBatchInfo, 16);
	}

	static uint32 GetClusterOffset()
	{
		return NANITE_GPU_PAGE_HEADER_SIZE;
	}
	uint32 GetMaterialTableOffset() const
	{
		return GetClusterOffset() + Cluster;
	}
	uint32 GetVertReuseBatchInfoOffset() const
	{
		return GetMaterialTableOffset() + GetMaterialTableSize();
	}
	uint32 GetDecodeInfoOffset() const
	{
		return GetVertReuseBatchInfoOffset() + GetVertReuseBatchInfoSize();
	}
	uint32 GetIndexOffset() const
	{
		return GetDecodeInfoOffset() + DecodeInfo;
	}
	uint32 GetPositionOffset() const
	{
		return GetIndexOffset() + Index;
	}
	uint32 GetAttributeOffset() const
	{
		return GetPositionOffset() + Position;
	}
	uint32 GetTotal() const
	{
		return GetAttributeOffset() + Attribute;
	}

	FPageSections GetOffsets() const
	{
		return FPageSections
		{
			GetClusterOffset(),
			GetMaterialTableOffset(),
			GetVertReuseBatchInfoOffset(),
			GetDecodeInfoOffset(),
			GetIndexOffset(),
			GetPositionOffset(),
			GetAttributeOffset()
		};
	}
	void operator+=(const FPageSections& Other)
	{
		Cluster += Other.Cluster;
		MaterialTable += Other.MaterialTable;
		VertReuseBatchInfo += Other.VertReuseBatchInfo;
		DecodeInfo += Other.DecodeInfo;
		Index += Other.Index;
		Position += Other.Position;
		Attribute += Other.Attribute;
	}
};

struct FUVRange
{
	FIntPoint Min = FIntPoint(ForceInit);
	FIntPoint GapStart = FIntPoint(ForceInit);
	FIntPoint GapLength = FIntPoint(ForceInit);
	int32 Precision = 0;
	int32 Pad = 0;

#if VOXEL_ENGINE_VERSION >= 504
#error "Update FUVRange"
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FEncodingSettings
{
	int32 PositionPrecision = 4;
	int32 UVBits = 8;
	static constexpr int32 NormalBits = 8;
};

struct FEncodingInfo
{
	FEncodingSettings Settings;

	int32 BitsPerIndex = 0;
	int32 BitsPerAttribute = 0;

	FIntVector PositionMin{ ForceInit };
	FIntVector PositionBits{ ForceInit };

	FColor ColorMin{ ForceInit };
	FColor ColorMax{ ForceInit };
	FIntVector4 ColorBits = FIntVector4(0);

	TVoxelArray<FUVRange, TFixedAllocator<NANITE_MAX_UVS>> UVRanges;
	TVoxelArray<float, TFixedAllocator<NANITE_MAX_UVS>> UVQuantizationScales;
	TVoxelArray<FIntPoint, TFixedAllocator<NANITE_MAX_UVS>> UVMins;

	FPageSections GpuSizes;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FCluster
{
public:
	TVoxelArray<FVector3f> Positions;
	TVoxelArray<FVoxelOctahedron> Normals;
	TVoxelArray<FColor> Colors;
	TVoxelArray<TVoxelArray<FVector2f>, TFixedAllocator<NANITE_MAX_UVS>> TextureCoordinates;

	FVoxelBox Bounds;
	float MaxEdgeLength = 0;

	FCluster();

	void ComputeBounds();
	FEncodingInfo ComputeEncodingInfo(const FEncodingSettings& Settings) const;
	FPackedCluster Pack(const FEncodingInfo& Info) const;

	FORCEINLINE int32 NumVertices() const
	{
		checkVoxelSlow(Positions.Num() == Normals.Num());
		return Positions.Num();
	}
	FORCEINLINE int32 NumTriangles() const
	{
		checkVoxelSlow(Positions.Num() % 3 == 0);
		return Positions.Num() / 3;
	}
	FORCEINLINE int32 NumMaterialBatches() const
	{
		const int32 MaxVerticesPerBatch = 32;
		// Only true because we don't reuse vertices
		const int32 TrianglesPerBatch = FMath::DivideAndRoundDown(MaxVerticesPerBatch, 3);
		return FMath::DivideAndRoundUp(NumTriangles(), TrianglesPerBatch);
	}
};

void CreatePageData(
	TVoxelArrayView<FCluster> Clusters,
	const FEncodingSettings& EncodingSettings,
	TVoxelChunkedArray<uint8>& PageData);
}