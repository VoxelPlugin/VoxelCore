// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNanite.h"
#include "Rendering/NaniteResources.h"

namespace Voxel::Nanite
{
FCluster::FCluster()
{
	Positions.Reserve(128);
	Normals.Reserve(128);
	Colors.Reserve(128);
}

void FCluster::ComputeBounds()
{
	VOXEL_FUNCTION_COUNTER();

	Bounds = FVoxelBox::FromPositions(Positions);

	VOXEL_SCOPE_COUNTER("MaxEdgeLength");

	float MaxEdgeLengthSquared = 0;
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles(); TriangleIndex++)
	{
		const FVector3f A = Positions[3 * TriangleIndex + 0];
		const FVector3f B = Positions[3 * TriangleIndex + 1];
		const FVector3f C = Positions[3 * TriangleIndex + 2];

		MaxEdgeLengthSquared = FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(A, B));
		MaxEdgeLengthSquared = FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(B, C));
		MaxEdgeLengthSquared = FMath::Max(MaxEdgeLengthSquared, FVector3f::DistSquared(A, C));
	}
	MaxEdgeLength = FMath::Sqrt(MaxEdgeLengthSquared);
}

FEncodingInfo FCluster::ComputeEncodingInfo(const FEncodingSettings& Settings) const
{
	VOXEL_FUNCTION_COUNTER();

	FEncodingInfo Info;
	Info.Settings = Settings;

	Info.BitsPerIndex = FMath::FloorLog2(NumVertices() - 1) + 1;
	Info.BitsPerAttribute = 2 * Settings.NormalBits;

	{
		const float QuantizationScale = FMath::Exp2(float(Settings.PositionPrecision));

		const FIntVector Min = FVoxelUtilities::FloorToInt(Bounds.Min * QuantizationScale);
		const FIntVector Max = FVoxelUtilities::CeilToInt(Bounds.Max * QuantizationScale);

		Info.PositionMin = Min;
		Info.PositionBits.X = FMath::CeilLogTwo(Max.X - Min.X + 1);
		Info.PositionBits.Y = FMath::CeilLogTwo(Max.Y - Min.Y + 1);
		Info.PositionBits.Z = FMath::CeilLogTwo(Max.Z - Min.Z + 1);

		if (!ensureMsgf(Info.PositionBits.GetMax() <= NANITE_MAX_POSITION_QUANTIZATION_BITS, TEXT("PositionPrecision too high")))
		{
			Info.PositionBits = FIntVector(NANITE_MAX_POSITION_QUANTIZATION_BITS);
		}
	}

	if (Colors.Num() > 0)
	{
		FVoxelUtilities::GetMinMax(Colors, Info.ColorMin, Info.ColorMax);

		if (Info.ColorMin == Info.ColorMax)
		{
			Info.ColorBits = FIntVector4(0);
		}
		else
		{
			Info.ColorBits.X = FMath::CeilLogTwo(int32(Info.ColorMax.R) - int32(Info.ColorMin.R) + 1);
			Info.ColorBits.Y = FMath::CeilLogTwo(int32(Info.ColorMax.G) - int32(Info.ColorMin.G) + 1);
			Info.ColorBits.Z = FMath::CeilLogTwo(int32(Info.ColorMax.B) - int32(Info.ColorMin.B) + 1);
			Info.ColorBits.W = FMath::CeilLogTwo(int32(Info.ColorMax.A) - int32(Info.ColorMin.A) + 1);

			Info.BitsPerAttribute += Info.ColorBits.X;
			Info.BitsPerAttribute += Info.ColorBits.Y;
			Info.BitsPerAttribute += Info.ColorBits.Z;
			Info.BitsPerAttribute += Info.ColorBits.W;
		}
	}

	for (int32 UVIndex = 0; UVIndex < TextureCoordinates.Num(); UVIndex++)
	{
		Info.BitsPerAttribute += 2 * Settings.UVBits;

		FUVRange& UVRange = Info.UVRanges.Emplace_GetRef();
		UVRange.GapStart = FIntPoint(MAX_int32);

		FVector2f Min{ ForceInit };
		FVector2f Max{ ForceInit };
		FVoxelUtilities::GetMinMax(TextureCoordinates[UVIndex], Min, Max);

		const float Size = (Max - Min).GetMax();
		const float PerfectStep = Size / ((1 << Settings.UVBits) - 1);
		const int32 InversePrecision = FMath::CeilToInt(FMath::Log2(PerfectStep));
		const float Step = FMath::Exp2(float(InversePrecision));

		UVRange.Min = FVoxelUtilities::FloorToInt(Min / Step);
		UVRange.Precision = -InversePrecision;

		Info.UVQuantizationScales.Add(FMath::Exp2(float(UVRange.Precision)));
		Info.UVMins.Add(UVRange.Min);
	}

	FPageSections& GpuSizes = Info.GpuSizes;
	GpuSizes.Cluster = sizeof(FPackedCluster);
	GpuSizes.MaterialTable = 0;
	GpuSizes.VertReuseBatchInfo = 0;
	GpuSizes.DecodeInfo = TextureCoordinates.Num() * sizeof(FUVRange);

	// TODO Not true if we don't reuse vertices?
	const int32 BitsPerTriangle = Info.BitsPerIndex + 2 * 5; // Base index + two 5-bit offsets
	GpuSizes.Index = FMath::DivideAndRoundUp(NumTriangles() * BitsPerTriangle, 32) * sizeof(uint32);

	const int32 PositionBitsPerVertex =
		Info.PositionBits.X +
		Info.PositionBits.Y +
		Info.PositionBits.Z;

	GpuSizes.Position = FMath::DivideAndRoundUp(NumVertices() * PositionBitsPerVertex, 32) * sizeof(uint32);
	GpuSizes.Attribute = FMath::DivideAndRoundUp(NumVertices() * Info.BitsPerAttribute, 32) * sizeof(uint32);

	return Info;
}

FPackedCluster FCluster::Pack(const FEncodingInfo& Info) const
{
	VOXEL_FUNCTION_COUNTER();

	FPackedCluster Result;
	FMemory::Memzero(Result);

	Result.SetNumVerts(NumVertices());
	Result.SetNumTris(NumTriangles());

	if (Colors.Num() == 0)
	{
		Result.SetColorMode(NANITE_VERTEX_COLOR_MODE_WHITE);
	}
	else if (Info.ColorBits == FIntVector4(0))
	{
		Result.SetColorMode(NANITE_VERTEX_COLOR_MODE_CONSTANT);
		Result.ColorMin = Info.ColorMin.ToPackedABGR();
	}
	else
	{
		Result.SetColorMode(NANITE_VERTEX_COLOR_MODE_VARIABLE);
		Result.SetColorBitsR(Info.ColorBits.X);
		Result.SetColorBitsG(Info.ColorBits.Y);
		Result.SetColorBitsB(Info.ColorBits.Z);
		Result.SetColorBitsA(Info.ColorBits.W);
		Result.ColorMin = Info.ColorMin.ToPackedABGR();
	}

	Result.SetGroupIndex(0);
	Result.SetBitsPerIndex(Info.BitsPerIndex);

	Result.PosStart = Info.PositionMin;
	Result.SetPosPrecision(Info.Settings.PositionPrecision);
	Result.SetPosBitsX(Info.PositionBits.X);
	Result.SetPosBitsY(Info.PositionBits.Y);
	Result.SetPosBitsZ(Info.PositionBits.Z);

	Result.LODBounds = FVector4f(
		Bounds.GetCenter().X,
		Bounds.GetCenter().Y,
		Bounds.GetCenter().Z,
		Bounds.Size().Length());

	Result.BoxBoundsCenter = FVector3f(Bounds.GetCenter());

	Result.LODErrorAndEdgeLength =
		(uint32(FFloat16(0.1f /* TODO? InCluster.LODError*/).Encoded) << 0) |
		(uint32(FFloat16(MaxEdgeLength).Encoded) << 16);

	Result.BoxBoundsExtent = FVector3f(Bounds.GetExtent());
	Result.Flags = UE_503_SWITCH(NANITE_CLUSTER_FLAG_LEAF, NANITE_CLUSTER_FLAG_STREAMING_LEAF | NANITE_CLUSTER_FLAG_ROOT_LEAF);

	Result.SetBitsPerAttribute(Info.BitsPerAttribute);
	Result.SetNormalPrecision(Info.Settings.NormalBits);
	Result.SetNumUVs(TextureCoordinates.Num());

	const int32 UVBits = Info.Settings.UVBits;
	check(UVBits < 16);

	for (int32 UVIndex = 0; UVIndex < TextureCoordinates.Num(); UVIndex++)
	{
		Result.UV_Prec |= ((UVBits << 4) | UVBits) << (UVIndex * 8);
	}

	return Result;
}

void CreatePageData(
	const TVoxelArrayView<FCluster> Clusters,
	const FEncodingSettings& EncodingSettings,
	TVoxelChunkedArray<uint8>& PageData)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(PageData.Num() % sizeof(uint32) == 0);

	const int32 PageStartIndex = PageData.Num();
	const auto GetPageOffset = [&]
	{
		return PageData.Num() - PageStartIndex;
	};

	const int32 NumUVs = Clusters[0].TextureCoordinates.Num();

	for (FCluster& Cluster : Clusters)
	{
		check(Cluster.NumTriangles() <= NANITE_MAX_CLUSTER_TRIANGLES);
		check(Cluster.TextureCoordinates.Num() == NumUVs);

		Cluster.ComputeBounds();
	}

	TVoxelArray<FEncodingInfo> EncodingInfos;
	EncodingInfos.Reserve(Clusters.Num());
	for (const FCluster& Cluster : Clusters)
	{
		EncodingInfos.Add(Cluster.ComputeEncodingInfo(EncodingSettings));
	}

	FPageSections PageGpuSizes;
	for (const FEncodingInfo& EncodingInfo : EncodingInfos)
	{
		PageGpuSizes += EncodingInfo.GpuSizes;
	}

	FPageSections GpuSectionOffsets = PageGpuSizes.GetOffsets();

	TVoxelArray<FPackedCluster> PackedClusters;
	PackedClusters.Reserve(Clusters.Num());
	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
	{
		const FCluster& Cluster = Clusters[ClusterIndex];
		const FEncodingInfo& Info = EncodingInfos[ClusterIndex];

		FPackedCluster& PackedCluster = PackedClusters.Emplace_GetRef();
		PackedCluster = Cluster.Pack(Info);

		constexpr int32 NumBits_BatchCount = 4;
		const int32 NumBatches = Cluster.NumMaterialBatches();

		FVoxelBitWriter BitWriter;
		// Inline count for 3 material ranges (we only have one)
		BitWriter.Append(NumBatches, NumBits_BatchCount);
		BitWriter.Append(0, NumBits_BatchCount);
		BitWriter.Append(0, NumBits_BatchCount);

		int32 NumTrianglesLeft = Cluster.NumTriangles();
		for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
		{
			const int32 MaxVerticesPerBatch = 32;
			// Only true because we don't reuse vertices
			const int32 TrianglesPerBatch = FMath::DivideAndRoundDown(MaxVerticesPerBatch, 3);
			const int32 NumTrianglesInBatch = FMath::Min(NumTrianglesLeft, TrianglesPerBatch);
			constexpr int32 NumBits_TriangleCount = 5;

			BitWriter.Append(NumTrianglesInBatch - 1, NumBits_TriangleCount);
			NumTrianglesLeft -= NumTrianglesInBatch;
		}
		checkVoxelSlow(NumTrianglesLeft == 0);

		BitWriter.Flush(sizeof(uint32));

		// See UnpackCluster
		PackedCluster.PackedMaterialInfo = uint32(Cluster.NumTriangles() - 1) << 18;

		checkVoxelSlow(GpuSectionOffsets.Index % 4 == 0);
		checkVoxelSlow(GpuSectionOffsets.Position % 4 == 0);
		checkVoxelSlow(GpuSectionOffsets.Attribute % 4 == 0);
		PackedCluster.SetIndexOffset(GpuSectionOffsets.Index);
		PackedCluster.SetPositionOffset(GpuSectionOffsets.Position);
		PackedCluster.SetAttributeOffset(GpuSectionOffsets.Attribute);
		PackedCluster.SetDecodeInfoOffset(GpuSectionOffsets.DecodeInfo);

		TArray<uint32> BatchInfo(BitWriter.GetWordData());
		PackedCluster.SetVertResourceBatchInfo(
			BatchInfo,
			GpuSectionOffsets.VertReuseBatchInfo,
			1);

		GpuSectionOffsets += Info.GpuSizes;
	}
	checkVoxelSlow(GpuSectionOffsets.Cluster == PageGpuSizes.GetMaterialTableOffset());
	checkVoxelSlow(Align(GpuSectionOffsets.MaterialTable, 16) == PageGpuSizes.GetVertReuseBatchInfoOffset());
	checkVoxelSlow(Align(GpuSectionOffsets.VertReuseBatchInfo, 16) == PageGpuSizes.GetDecodeInfoOffset());
	checkVoxelSlow(GpuSectionOffsets.DecodeInfo == PageGpuSizes.GetIndexOffset());
	checkVoxelSlow(GpuSectionOffsets.Index == PageGpuSizes.GetPositionOffset());
	checkVoxelSlow(GpuSectionOffsets.Position == PageGpuSizes.GetAttributeOffset());
	checkVoxelSlow(GpuSectionOffsets.Attribute == PageGpuSizes.GetTotal());

	TVoxelChunkedRef<FPageDiskHeader> PageDiskHeader = AllocateChunkedRef(PageData);
	PageDiskHeader->NumClusters = Clusters.Num();
	PageDiskHeader->GpuSize = PageGpuSizes.GetTotal();
	PageDiskHeader->NumRawFloat4s =
		sizeof(FPageGPUHeader) / 16 +
		Clusters.Num() * (sizeof(FPackedCluster) + NumUVs * sizeof(FUVRange)) / 16;

	PageDiskHeader->NumTexCoords = NumUVs;

	TVoxelChunkedArrayRef<FClusterDiskHeader> ClusterDiskHeaders = AllocateChunkedArrayRef(PageData, Clusters.Num());

	{
		TVoxelChunkedRef<FPageGPUHeader> GPUPageHeader = AllocateChunkedRef(PageData);
		GPUPageHeader->NumClusters = Clusters.Num();
	}

	{
		checkStatic(sizeof(FPackedCluster) % 16 == 0);
		constexpr int32 VectorPerCluster = sizeof(FPackedCluster) / 16;

		const TConstVoxelArrayView<FVector4f> VectorArray = ReinterpretCastVoxelArrayView<FVector4f>(PackedClusters);
		for (int32 VectorIndex = 0; VectorIndex < VectorPerCluster; VectorIndex++)
		{
			for (int32 ClusterIndex = 0; ClusterIndex < PackedClusters.Num(); ClusterIndex++)
			{
				PageData.Append(MakeByteVoxelArrayView(VectorArray[ClusterIndex * VectorPerCluster + VectorIndex]));
			}
		}
	}

	{
		PageDiskHeader->DecodeInfoOffset = GetPageOffset();

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			const FEncodingInfo& Info = EncodingInfos[ClusterIndex];

			for (int32 UVIndex = 0; UVIndex < NumUVs; UVIndex++)
			{
				PageData.Append(MakeByteVoxelArrayView(Info.UVRanges[UVIndex]));
			}
		}
	}

	// Index data
	{
		checkStatic(NANITE_USE_STRIP_INDICES);

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			const FCluster& Cluster = Clusters[ClusterIndex];
			FClusterDiskHeader& ClusterDiskHeader = ClusterDiskHeaders[ClusterIndex];

			TVoxelStaticArray<uint32, 4> NumNewVerticesInDword{ ForceInit };
			TVoxelStaticArray<uint32, 4> NumRefVerticesInDword{ ForceInit };
			for (int32 TriangleIndex = 0; TriangleIndex < Cluster.NumTriangles(); TriangleIndex++)
			{
				NumNewVerticesInDword[TriangleIndex >> 5] += 3;
			}

			{
				uint32 NumBeforeDwords1 = NumNewVerticesInDword[0];
				uint32 NumBeforeDwords2 = NumNewVerticesInDword[1] + NumBeforeDwords1;
				uint32 NumBeforeDwords3 = NumNewVerticesInDword[2] + NumBeforeDwords2;

				checkVoxelSlow(NumBeforeDwords1 < 1024);
				checkVoxelSlow(NumBeforeDwords2 < 1024);
				checkVoxelSlow(NumBeforeDwords3 < 1024);

				ClusterDiskHeader.NumPrevNewVerticesBeforeDwords =
					(NumBeforeDwords3 << 20) |
					(NumBeforeDwords2 << 10) |
					(NumBeforeDwords1 << 0);
			}

			{
				uint32 NumBeforeDwords1 = NumRefVerticesInDword[0];
				uint32 NumBeforeDwords2 = NumRefVerticesInDword[1] + NumBeforeDwords1;
				uint32 NumBeforeDwords3 = NumRefVerticesInDword[2] + NumBeforeDwords2;

				checkVoxelSlow(NumBeforeDwords1 < 1024);
				checkVoxelSlow(NumBeforeDwords2 < 1024);
				checkVoxelSlow(NumBeforeDwords3 < 1024);

				ClusterDiskHeader.NumPrevRefVerticesBeforeDwords =
					(NumBeforeDwords3 << 20) |
					(NumBeforeDwords2 << 10) |
					(NumBeforeDwords1 << 0);
			}

			ClusterDiskHeader.IndexDataOffset = GetPageOffset();
			// No vertex reuse PagePointer.Append(Cluster.StripIndexData);
		}
		while (PageData.Num() % sizeof(uint32) != 0)
		{
			PageData.Add(0);
		}

		PageDiskHeader->StripBitmaskOffset = GetPageOffset();

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			checkStatic(NANITE_USE_STRIP_INDICES);
			checkStatic(!NANITE_USE_UNCOMPRESSED_VERTEX_DATA);

			const int32 NumDwords = NANITE_MAX_CLUSTER_TRIANGLES / 32;

			TVoxelChunkedArrayRef<uint32> Bitmasks = AllocateChunkedArrayRef(PageData, 3 * NumDwords);

			// See UnpackStripIndices
			for (int32 Index = 0; Index < NumDwords; Index++)
			{
				// Always start of new strip
				Bitmasks[3 * Index + 0] = 0xFFFFFFFF;
				// Never reuse vertices
				Bitmasks[3 * Index + 1] = 0;
				Bitmasks[3 * Index + 2] = 0;
			}
		}
	}

	// Write PageCluster Map
	for (FClusterDiskHeader& ClusterDiskHeader : ClusterDiskHeaders)
	{
		ClusterDiskHeader.PageClusterMapOffset = GetPageOffset();
	}

	{
		PageDiskHeader->VertexRefBitmaskOffset = GetPageOffset();

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			PageData.AddZeroed((NANITE_MAX_CLUSTER_VERTICES / 32) * sizeof(uint32));
		}
	}

	// Write Vertex References (no vertex ref)
	{
		PageDiskHeader->NumVertexRefs = 0;

		for (FClusterDiskHeader& ClusterDiskHeader : ClusterDiskHeaders)
		{
			ClusterDiskHeader.VertexRefDataOffset = GetPageOffset();
			ClusterDiskHeader.NumVertexRefs = 0;
		}
		while (PageData.Num() % sizeof(uint32) != 0)
		{
			PageData.Add(0);
		}
	}

	{
		VOXEL_SCOPE_COUNTER("Write Positions");

		const float QuantizationScale = FMath::Exp2(float(EncodingSettings.PositionPrecision));

		FVoxelBitWriter BitWriter_Position;

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			const FCluster& Cluster = Clusters[ClusterIndex];
			const FEncodingInfo& Info = EncodingInfos[ClusterIndex];
			FClusterDiskHeader& ClusterDiskHeader = ClusterDiskHeaders[ClusterIndex];

			BitWriter_Position.Reset();

			for (const FVector3f& Position : Cluster.Positions)
			{
				const FIntVector IntPosition = FVoxelUtilities::RoundToInt(Position * QuantizationScale) - Info.PositionMin;

				BitWriter_Position.Append(IntPosition.X, Info.PositionBits.X);
				BitWriter_Position.Append(IntPosition.Y, Info.PositionBits.Y);
				BitWriter_Position.Append(IntPosition.Z, Info.PositionBits.Z);
				BitWriter_Position.Flush(1);
			}
			BitWriter_Position.Flush(sizeof(uint32));

			ClusterDiskHeader.PositionDataOffset = GetPageOffset();
			PageData.Append(BitWriter_Position.GetByteData());
		}
	}

	{
		VOXEL_SCOPE_COUNTER("Write Attributes");

		FVoxelBitWriter BitWriter_Attribute;

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			const FCluster& Cluster = Clusters[ClusterIndex];
			const FEncodingInfo& Info = EncodingInfos[ClusterIndex];

			BitWriter_Attribute.Reset();

			// Quantize and write remaining shading attributes
			for (int32 VertexIndex = 0; VertexIndex < Cluster.Positions.Num(); VertexIndex++)
			{
				// Normal
				checkStatic(FEncodingSettings::NormalBits == 8);
				const uint32 PackedNormal = ReinterpretCastRef<uint16>(Cluster.Normals[VertexIndex]);
				BitWriter_Attribute.Append(PackedNormal, 2 * EncodingSettings.NormalBits);

				// Color
				if (Cluster.Colors.Num() > 0)
				{
					const FColor Color = Cluster.Colors[VertexIndex];

					const int32 R = Color.R - Info.ColorMin.R;
					const int32 G = Color.G - Info.ColorMin.G;
					const int32 B = Color.B - Info.ColorMin.B;
					const int32 A = Color.A - Info.ColorMin.A;
					BitWriter_Attribute.Append(R, Info.ColorBits.X);
					BitWriter_Attribute.Append(G, Info.ColorBits.Y);
					BitWriter_Attribute.Append(B, Info.ColorBits.Z);
					BitWriter_Attribute.Append(A, Info.ColorBits.W);
				}

				// UVs
				for (int32 UVIndex = 0; UVIndex < NumUVs; UVIndex++)
				{
					const FVector2f UV = Cluster.TextureCoordinates[UVIndex][VertexIndex];
					const int32 U = FMath::RoundToInt(UV.X * Info.UVQuantizationScales[UVIndex]) - Info.UVMins[UVIndex].X;
					const int32 V = FMath::RoundToInt(UV.Y * Info.UVQuantizationScales[UVIndex]) - Info.UVMins[UVIndex].Y;

					BitWriter_Attribute.Append(U, EncodingSettings.UVBits);
					BitWriter_Attribute.Append(V, EncodingSettings.UVBits);
				}
				BitWriter_Attribute.Flush(1);
			}
			BitWriter_Attribute.Flush(sizeof(uint32));

			ClusterDiskHeaders[ClusterIndex].AttributeDataOffset = GetPageOffset();
			PageData.Append(BitWriter_Attribute.GetByteData());
		}
	}

	check(PageData.Num() < NANITE_MAX_PAGE_DISK_SIZE);
}
}