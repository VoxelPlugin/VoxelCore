// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelNanite.h"
#include "Rendering/NaniteResources.h"

namespace Voxel::Nanite
{
FORCEINLINE uint32 EncodeZigZag(const int32 Value)
{
	return uint32((Value << 1) ^ (Value >> 31));
}
template<typename T>
uint32 EncodeZigZag(T) = delete;

FORCEINLINE int32 ShortestWrap(int32 Value, const int32 NumBits)
{
	if (NumBits == 0)
	{
		ensureVoxelSlow(Value == 0);
		return 0;
	}

	const int32 Shift = 32 - NumBits;
	const int32 NumValues = (1 << NumBits);
	const int32 MinValue = -(NumValues >> 1);
	const int32 MaxValue = (NumValues >> 1) - 1;

	Value = (Value << Shift) >> Shift;
	checkVoxelSlow(Value >= MinValue && Value <= MaxValue);

	return Value;
}

FORCEINLINE uint32 EncodeUVFloat(const float Value, const uint32 NumMantissaBits)
{
	// Encode UV floats as a custom float type where [0,1] is denormal, so it gets uniform precision.
	// As UVs are encoded in clusters as ranges of encoded values, a few modifications to the usual
	// float encoding are made to preserve the original float order when the encoded values are interpreted as uints:
	// 1. Positive values use 1 as sign bit.
	// 2. Negative values use 0 as sign bit and have their exponent and mantissa bits inverted.

	checkVoxelSlow(FMath::IsFinite(Value));

	const uint32 SignBitPosition = NANITE_UV_FLOAT_NUM_EXPONENT_BITS + NumMantissaBits;
	const uint32 FloatUInt = FVoxelUtilities::IntBits(Value);
	const uint32 AbsFloatUInt = FloatUInt & 0x7FFFFFFFu;

	uint32 Result;
	if (AbsFloatUInt < 0x3F800000u)
	{
		// Denormal encoding
		// Note: Mantissa can overflow into first non-denormal value (1.0f),
		// but that is desirable to get correct round-to-nearest behavior.
		const float AbsFloat = FVoxelUtilities::FloatBits(AbsFloatUInt);
		Result = uint32(double(AbsFloat * float(1u << NumMantissaBits)) + 0.5);	// Cast to double to make sure +0.5 is lossless
	}
	else
	{
		// Normal encoding
		// Extract exponent and mantissa bits from 32-bit float
		const uint32 Shift = 23 - NumMantissaBits;
		const uint32 Tmp = (AbsFloatUInt - 0x3F000000u) + (1u << (Shift - 1));	// Bias to round to nearest
		Result = FMath::Min(Tmp >> Shift, (1u << SignBitPosition) - 1u);		// Clamp to largest UV float value
	}

	// Produce a mask that for positive values only flips the sign bit
	// and for negative values only flips the exponent and mantissa bits.
	const uint32 SignMask = (1u << SignBitPosition) - (FloatUInt >> 31u);
	Result ^= SignMask;

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FCluster::FCluster()
{
	Positions.Reserve(128);
	Normals.Reserve(128);
	Colors.Reserve(128);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox FCluster::GetBounds() const
{
	if (!CachedBounds.IsSet())
	{
		VOXEL_FUNCTION_COUNTER();

		CachedBounds = FVoxelBox::FromPositions(Positions);
	}
	return CachedBounds.GetValue();
}

const FEncodingInfo& FCluster::GetEncodingInfo(const FEncodingSettings& Settings) const
{
	if (CachedEncodingInfo.IsSet())
	{
		return CachedEncodingInfo.GetValue();
	}

	VOXEL_FUNCTION_COUNTER();

	const FVoxelBox Bounds = GetBounds();

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

		if (Info.PositionBits.GetMax() > NANITE_MAX_POSITION_QUANTIZATION_BITS)
		{
			VOXEL_MESSAGE(Error, "PositionPrecision too high on voxel Nanite mesh");

			Info.PositionBits = FVoxelUtilities::ComponentMin(Info.PositionBits, FIntVector(NANITE_MAX_POSITION_QUANTIZATION_BITS));
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
		VOXEL_SCOPE_COUNTER("UVs");

		FUVRange& UVRange = Info.UVRanges.Emplace_GetRef();
		FUintVector2 UVMin = FUintVector2(0xFFFFFFFFu, 0xFFFFFFFFu);
		FUintVector2 UVMax = FUintVector2(0u, 0u);

		for (const FVector2f& UV : TextureCoordinates[UVIndex])
		{
			const uint32 EncodedU = EncodeUVFloat(UV.X, NANITE_UV_FLOAT_NUM_MANTISSA_BITS);
			const uint32 EncodedV = EncodeUVFloat(UV.Y, NANITE_UV_FLOAT_NUM_MANTISSA_BITS);

			UVMin.X = FMath::Min(UVMin.X, EncodedU);
			UVMin.Y = FMath::Min(UVMin.Y, EncodedV);
			UVMax.X = FMath::Max(UVMax.X, EncodedU);
			UVMax.Y = FMath::Max(UVMax.Y, EncodedV);
		}

		const FUintVector2 UVDelta = UVMax - UVMin;

		UVRange.Min = UVMin;
		UVRange.NumBits.X = FMath::CeilLogTwo(UVDelta.X + 1u);
		UVRange.NumBits.Y = FMath::CeilLogTwo(UVDelta.Y + 1u);

		Info.BitsPerAttribute += UVRange.NumBits.X + UVRange.NumBits.Y;
		Info.UVMins.Add(UVRange.Min);
	}

	FPageSections& GpuSizes = Info.GpuSizes;
	GpuSizes.Cluster = sizeof(FPackedCluster);
	GpuSizes.MaterialTable = 0;
	GpuSizes.VertReuseBatchInfo = 0;
	GpuSizes.DecodeInfo = TextureCoordinates.Num() * sizeof(FPackedUVRange);

	const int32 BitsPerTriangle = Info.BitsPerIndex + 2 * 5; // Base index + two 5-bit offsets
	GpuSizes.Index = FMath::DivideAndRoundUp(NumTriangles() * BitsPerTriangle, 32) * sizeof(uint32);

	const int32 PositionBitsPerVertex =
		Info.PositionBits.X +
		Info.PositionBits.Y +
		Info.PositionBits.Z;

	GpuSizes.Position = FMath::DivideAndRoundUp(NumVertices() * PositionBitsPerVertex, 32) * sizeof(uint32);
	GpuSizes.Attribute = FMath::DivideAndRoundUp(NumVertices() * Info.BitsPerAttribute, 32) * sizeof(uint32);

	CachedEncodingInfo = Info;

	return CachedEncodingInfo.GetValue();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FPackedCluster FCluster::Pack(const FEncodingInfo& Info) const
{
	VOXEL_FUNCTION_COUNTER();

	const FVoxelBox Bounds = GetBounds();

	const float MaxEdgeLength = INLINE_LAMBDA
	{
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
		return FMath::Sqrt(MaxEdgeLengthSquared);
	};

	FPackedCluster Result;
	FMemory::Memzero(Result);

	Result.SetNumVerts(NumVertices());
	Result.SetNumTris(NumTriangles());

	if (Colors.Num() == 0)
	{
		Result.SetColorMode(NANITE_VERTEX_COLOR_MODE_CONSTANT);
		Result.ColorMin = FColor::White.ToPackedABGR();
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

#if VOXEL_ENGINE_VERSION >= 506
	Result.LODBounds = FSphere3f(FVector3f(
			Bounds.GetCenter().X,
			Bounds.GetCenter().Y,
			Bounds.GetCenter().Z),
		Bounds.Size().Length());
#else
	Result.LODBounds = FVector4f(
		Bounds.GetCenter().X,
		Bounds.GetCenter().Y,
		Bounds.GetCenter().Z,
		Bounds.Size().Length());
#endif

	Result.BoxBoundsCenter = FVector3f(Bounds.GetCenter());

	Result.LODErrorAndEdgeLength =
		(uint32(FFloat16(0.1f /* TODO? InCluster.LODError*/).Encoded) << 0) |
		(uint32(FFloat16(MaxEdgeLength).Encoded) << 16);

	Result.BoxBoundsExtent = FVector3f(Bounds.GetExtent());
	Result.UE_506_SWITCH(Flags, Flags_NumClusterBoneInfluences) = NANITE_CLUSTER_FLAG_STREAMING_LEAF | NANITE_CLUSTER_FLAG_ROOT_LEAF;

	Result.SetBitsPerAttribute(Info.BitsPerAttribute);
	Result.SetNormalPrecision(Info.Settings.NormalBits);
	Result.SetHasTangents(false);
	Result.SetNumUVs(TextureCoordinates.Num());

	check(TextureCoordinates.Num() <= NANITE_MAX_UVS);

	uint32 BitOffset = 0;
	for (int32 UVIndex = 0; UVIndex < TextureCoordinates.Num(); UVIndex++)
	{
		checkVoxelSlow(BitOffset < 256);
		Result.UVBitOffsets |= BitOffset << (UVIndex * 8);

		const FUVRange& UVRange = Info.UVRanges[UVIndex];
		BitOffset += UVRange.NumBits.X + UVRange.NumBits.Y;
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CreatePageData(
	const TVoxelArrayView<FCluster> Clusters,
	const FEncodingSettings& EncodingSettings,
	TVoxelChunkedArray<uint8>& PageData,
	int32& VertexOffset)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(PageData.Num() % sizeof(uint32) == 0);

	const int32 PageStartIndex = PageData.Num();
	const auto GetPageOffset = [&]
	{
		return PageData.Num() - PageStartIndex;
	};

	const int32 StartVertexOffset = VertexOffset;
	const int32 NumUVs = Clusters[0].TextureCoordinates.Num();

	for (FCluster& Cluster : Clusters)
	{
		check(Cluster.NumTriangles() <= NANITE_MAX_CLUSTER_TRIANGLES);
		check(Cluster.TextureCoordinates.Num() == NumUVs);
	}

	TVoxelArray<FEncodingInfo> EncodingInfos;
	EncodingInfos.Reserve(Clusters.Num());
	for (const FCluster& Cluster : Clusters)
	{
		EncodingInfos.Add(Cluster.GetEncodingInfo(EncodingSettings));
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

		{
			const int32 RelativeVertexOffset = VertexOffset - StartVertexOffset;
			ensure(FVoxelUtilities::IsValidUINT16(RelativeVertexOffset));
			PackedCluster.SetGroupIndex(RelativeVertexOffset);
		}
		VertexOffset += Cluster.NumVertices();

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

	ensure(PageGpuSizes.GetTotal() <= NANITE_ROOT_PAGE_GPU_SIZE);

	TVoxelChunkedRef<FPageDiskHeader> PageDiskHeader = AllocateChunkedRef(PageData);
	PageDiskHeader->NumClusters = Clusters.Num();

	TVoxelChunkedArrayRef<FClusterDiskHeader> ClusterDiskHeaders = AllocateChunkedArrayRef(PageData, Clusters.Num());

	const int32 RawFloat4StartOffset = GetPageOffset();

	{
		TVoxelChunkedRef<FPageGPUHeader> GPUPageHeader = AllocateChunkedRef(PageData);
		GPUPageHeader->NumClusters = Clusters.Num();
	}

	{
		checkStatic(sizeof(FPackedCluster) % 16 == 0);
		constexpr int32 VectorPerCluster = sizeof(FPackedCluster) / 16;

		const TConstVoxelArrayView<FVector4f> VectorArray = MakeVoxelArrayView(PackedClusters).ReinterpretAs<FVector4f>();
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
				const FUVRange UVRange = Info.UVRanges[UVIndex];

				checkVoxelSlow(UVRange.NumBits.X <= NANITE_UV_FLOAT_MAX_BITS && UVRange.NumBits.Y <= NANITE_UV_FLOAT_MAX_BITS);
				checkVoxelSlow(UVRange.Min.X < (1u << NANITE_UV_FLOAT_MAX_BITS) && UVRange.Min.Y < (1u << NANITE_UV_FLOAT_MAX_BITS));

				FPackedUVRange PackedUVRange;
				PackedUVRange.Data.X = (UVRange.Min.X << 5) | UVRange.NumBits.X;
				PackedUVRange.Data.Y = (UVRange.Min.Y << 5) | UVRange.NumBits.Y;
				PageData.Append(MakeByteVoxelArrayView(PackedUVRange));
			}
		}

		while ((GetPageOffset() - PageDiskHeader->DecodeInfoOffset) % 16 != 0)
		{
			PageData.Add(0);
		}
	}

	const int32 RawFloat4EndOffset = GetPageOffset();

	checkVoxelSlow((RawFloat4EndOffset - RawFloat4StartOffset) % sizeof(FVector4f) == 0);
	PageDiskHeader->NumRawFloat4s = (RawFloat4EndOffset - RawFloat4StartOffset) / sizeof(FVector4f);

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
		VOXEL_SCOPE_COUNTER("Write Attributes");

		struct FByteStreamCounters
		{
			int32 Low = 0;
			int32 Mid = 0;
			int32 High = 0;
		};
		TVoxelArray<FByteStreamCounters> ByteStreamCounters;
		ByteStreamCounters.SetNum(Clusters.Num());

		TVoxelChunkedArray<uint8> LowByteStream;
		TVoxelChunkedArray<uint8> MidByteStream;
		TVoxelChunkedArray<uint8> HighByteStream;

		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ClusterIndex++)
		{
			const FCluster& Cluster = Clusters[ClusterIndex];
			const FEncodingInfo& Info = EncodingInfos[ClusterIndex];

			const int32 PrevLow = LowByteStream.Num();
			const int32 PrevMid = MidByteStream.Num();
			const int32 PrevHigh = HighByteStream.Num();
			ON_SCOPE_EXIT
			{
				ByteStreamCounters[ClusterIndex].Low = LowByteStream.Num() - PrevLow;
				ByteStreamCounters[ClusterIndex].Mid = MidByteStream.Num() - PrevMid;
				ByteStreamCounters[ClusterIndex].High = HighByteStream.Num() - PrevHigh;
			};

			const auto WriteZigZagDelta = [&](const int32 Delta, const int32 NumBytes)
			{
				const uint32 Value = EncodeZigZag(Delta);

				checkVoxelSlow(NumBytes <= 3);
				checkVoxelSlow(Value < (1u << (NumBytes * 8)));

				if (NumBytes >= 3)
				{
					HighByteStream.Add((Value >> 16) & 0xFFu);
				}

				if (NumBytes >= 2)
				{
					MidByteStream.Add((Value >> 8) & 0xFFu);
				}

				if (NumBytes >= 1)
				{
					LowByteStream.Add(Value & 0xFFu);
				}
			};

			const int32 BytesPerPositionComponent = FMath::DivideAndRoundUp(Info.PositionBits.GetMax(), 8);
			const int32 BytesPerNormalComponent = FMath::DivideAndRoundUp(EncodingSettings.NormalBits, 8);

			{
				const float QuantizationScale = FMath::Exp2(float(EncodingSettings.PositionPrecision));

				FIntVector PrevPosition = FIntVector(
					(1 << Info.PositionBits.X) / 2,
					(1 << Info.PositionBits.Y) / 2,
					(1 << Info.PositionBits.Z) / 2);

				for (const FVector3f& FloatPosition : Cluster.Positions)
				{
					const FIntVector Position = FVoxelUtilities::RoundToInt(FloatPosition * QuantizationScale) - Info.PositionMin;
					FIntVector PositionDelta = Position - PrevPosition;

					PositionDelta.X = ShortestWrap(PositionDelta.X, Info.PositionBits.X);
					PositionDelta.Y = ShortestWrap(PositionDelta.Y, Info.PositionBits.Y);
					PositionDelta.Z = ShortestWrap(PositionDelta.Z, Info.PositionBits.Z);

					WriteZigZagDelta(PositionDelta.X, BytesPerPositionComponent);
					WriteZigZagDelta(PositionDelta.Y, BytesPerPositionComponent);
					WriteZigZagDelta(PositionDelta.Z, BytesPerPositionComponent);

					PrevPosition = Position;
				}
			}

			{
				FIntPoint PrevNormal = FIntPoint::ZeroValue;
				for (const FVoxelOctahedron& PackedNormal : Cluster.Normals)
				{
					const FIntPoint Normal = FIntPoint(PackedNormal.X, PackedNormal.Y);
					FIntPoint NormalDelta = Normal - PrevNormal;

					NormalDelta.X = ShortestWrap(NormalDelta.X, EncodingSettings.NormalBits);
					NormalDelta.Y = ShortestWrap(NormalDelta.Y, EncodingSettings.NormalBits);

					WriteZigZagDelta(NormalDelta.X, BytesPerNormalComponent);
					WriteZigZagDelta(NormalDelta.Y, BytesPerNormalComponent);

					PrevNormal = Normal;
				}
			}

			if (Cluster.Colors.Num() > 0 &&
				Info.ColorBits != FIntVector4(0))
			{
				FIntVector4 PrevColor = FIntVector4(0);
				for (const FColor& UnpackedColor : Cluster.Colors)
				{
					const FIntVector4 Color
					{
						UnpackedColor.R - Info.ColorMin.R,
						UnpackedColor.G - Info.ColorMin.G,
						UnpackedColor.B - Info.ColorMin.B,
						UnpackedColor.A - Info.ColorMin.A
					};
					FIntVector4 ColorDelta = Color - PrevColor;

					ColorDelta.X = ShortestWrap(ColorDelta.X, Info.ColorBits.X);
					ColorDelta.Y = ShortestWrap(ColorDelta.Y, Info.ColorBits.Y);
					ColorDelta.Z = ShortestWrap(ColorDelta.Z, Info.ColorBits.Z);
					ColorDelta.W = ShortestWrap(ColorDelta.W, Info.ColorBits.W);

					WriteZigZagDelta(ColorDelta.X, 1);
					WriteZigZagDelta(ColorDelta.Y, 1);
					WriteZigZagDelta(ColorDelta.Z, 1);
					WriteZigZagDelta(ColorDelta.W, 1);

					PrevColor = Color;
				}
			}

			for (int32 UVIndex = 0; UVIndex < NumUVs; UVIndex++)
			{
				const FUVRange& UVRange = Info.UVRanges[UVIndex];
				const int32 BytesPerTexCoordComponent = FMath::DivideAndRoundUp(FMath::Max<int32>(UVRange.NumBits.X, UVRange.NumBits.Y), 8);

				FIntVector2 PrevUV = FIntVector2::ZeroValue;
				for (const FVector2f& UnpackedUV : Cluster.TextureCoordinates[UVIndex])
				{
					uint32 EncodedU = EncodeUVFloat(UnpackedUV.X, NANITE_UV_FLOAT_NUM_MANTISSA_BITS);
					uint32 EncodedV = EncodeUVFloat(UnpackedUV.Y, NANITE_UV_FLOAT_NUM_MANTISSA_BITS);

					checkVoxelSlow(EncodedU >= UVRange.Min.X);
					checkVoxelSlow(EncodedV >= UVRange.Min.Y);
					EncodedU -= UVRange.Min.X;
					EncodedV -= UVRange.Min.Y;

					checkVoxelSlow(EncodedU < (1u << UVRange.NumBits.X));
					checkVoxelSlow(EncodedV < (1u << UVRange.NumBits.Y));

					const FIntVector2 UV
					{
						int32(EncodedU),
						int32(EncodedV)
					};
					FIntVector2 UVDelta = UV - PrevUV;

					UVDelta.X = ShortestWrap(UVDelta.X, UVRange.NumBits.X);
					UVDelta.Y = ShortestWrap(UVDelta.Y, UVRange.NumBits.Y);

					WriteZigZagDelta(UVDelta.X, BytesPerTexCoordComponent);
					WriteZigZagDelta(UVDelta.Y, BytesPerTexCoordComponent);

					PrevUV = UV;
				}
			}
		}

		// Write low/mid/high byte streams
		{
			{
				FClusterDiskHeader& Header = ClusterDiskHeaders[0];

				Header.LowBytesOffset = GetPageOffset();
				PageData.Append(LowByteStream);

				Header.MidBytesOffset = GetPageOffset();
				PageData.Append(MidByteStream);

				Header.HighBytesOffset = GetPageOffset();
				PageData.Append(HighByteStream);
			}

			for (int32 ClusterIndex = 1; ClusterIndex < Clusters.Num(); ClusterIndex++)
			{
				const FClusterDiskHeader& PrevHeader = ClusterDiskHeaders[ClusterIndex - 1];
				const FByteStreamCounters& PrevCounters = ByteStreamCounters[ClusterIndex - 1];

				FClusterDiskHeader& Header = ClusterDiskHeaders[ClusterIndex];
				Header.LowBytesOffset = PrevHeader.LowBytesOffset + PrevCounters.Low;
				Header.MidBytesOffset = PrevHeader.MidBytesOffset + PrevCounters.Mid;
				Header.HighBytesOffset = PrevHeader.HighBytesOffset + PrevCounters.High;
			}

			ensure(ClusterDiskHeaders[Clusters.Num() - 1].LowBytesOffset + ByteStreamCounters[Clusters.Num() - 1].Low == ClusterDiskHeaders[0].MidBytesOffset);
			ensure(ClusterDiskHeaders[Clusters.Num() - 1].MidBytesOffset + ByteStreamCounters[Clusters.Num() - 1].Mid == ClusterDiskHeaders[0].HighBytesOffset);
			ensure(ClusterDiskHeaders[Clusters.Num() - 1].HighBytesOffset + ByteStreamCounters[Clusters.Num() - 1].High == GetPageOffset());

			while (PageData.Num() % sizeof(uint32) != 0)
			{
				PageData.Add(0);
			}
		}
	}
}
}