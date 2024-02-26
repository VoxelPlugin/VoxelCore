// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelArrayUtilitiesImpl.ispc.generated.h"

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint8> Data, const uint8 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint8(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint16> Data, const uint16 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint16(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint32> Data, const uint32 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint32(Data.GetData(), Data.Num(), Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint64> Data, const uint64 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint64(Data.GetData(), Data.Num(), Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetMin(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMin(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetMax(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMax(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FInt32Interval FVoxelUtilities::GetMinMax(TConstVoxelArrayView<uint8> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_uint8(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FInt32Interval FVoxelUtilities::GetMinMax(TConstVoxelArrayView<uint16> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_uint16(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FInt32Interval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<int32> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_int32(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FFloatInterval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(0.f, 0.f);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMax_float(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FFloatInterval FVoxelUtilities::GetMinMaxSafe(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(MAX_flt, -MAX_flt);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_float(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

FDoubleInterval FVoxelUtilities::GetMinMaxSafe(const TConstVoxelArrayView<double> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (!ensure(Data.Num() > 0))
	{
		return FDoubleInterval(MAX_dbl, -MAX_dbl);
	}

	FDoubleInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_double(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

void FVoxelUtilities::FixupSignBit(const TVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num(), 1024);

	if (Data.Num() == 0)
	{
		return;
	}

	ispc::ArrayUtilities_FixupSignBit(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelOodleHeader
{
	uint64 Tag = MAKE_TAG_64("OODLE_VO");
	int64 UncompressedSize = 0;
	int64 CompressedSize = 0;
};

bool FVoxelUtilities::IsCompressedData(const TConstVoxelArrayView64<uint8> CompressedData)
{
	if (CompressedData.Num() < sizeof(FVoxelOodleHeader))
	{
		return false;
	}

	const TConstVoxelArrayView<uint8> HeaderBytes = MakeVoxelArrayView(CompressedData).LeftOf(sizeof(FVoxelOodleHeader));
	const FVoxelOodleHeader Header = FromByteVoxelArrayView<FVoxelOodleHeader>(HeaderBytes);

	if (Header.Tag != FVoxelOodleHeader().Tag)
	{
		return false;
	}

	return true;
}

TVoxelArray64<uint8> FVoxelUtilities::Compress(
	const TConstVoxelArrayView64<uint8> Data,
	const bool bAllowParallel,
	const FOodleDataCompression::ECompressor Compressor,
	const FOodleDataCompression::ECompressionLevel CompressionLevel)
{
	VOXEL_FUNCTION_COUNTER();

	if (Data.Num() == 0)
	{
		return {};
	}

	const int64 WorkingSizeNeeded = FOodleDataCompression::CompressedBufferSizeNeeded(Data.Num());

	TVoxelArray64<uint8> CompressedData;
	SetNumFast(CompressedData, sizeof(FVoxelOodleHeader) + WorkingSizeNeeded);

	int64 CompressedSize;
	if (bAllowParallel)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("CompressParallel %lldB %s %s",
			Data.Num(),
			ECompressorToString(Compressor),
			ECompressionLevelToString(CompressionLevel));

		CompressedSize = FOodleDataCompression::UE_503_SWITCH(Compress, CompressParallel)(
			CompressedData.GetData() + sizeof(FVoxelOodleHeader),
			WorkingSizeNeeded,
			Data.GetData(),
			Data.Num(),
			Compressor,
			CompressionLevel);
	}
	else
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Compress %lldB %s %s",
			Data.Num(),
			ECompressorToString(Compressor),
			ECompressionLevelToString(CompressionLevel));

		CompressedSize = FOodleDataCompression::Compress(
			CompressedData.GetData() + sizeof(FVoxelOodleHeader),
			WorkingSizeNeeded,
			Data.GetData(),
			Data.Num(),
			Compressor,
			CompressionLevel);
	}
	check(CompressedSize > 0);

	check(int64(sizeof(FVoxelOodleHeader)) + CompressedSize <= CompressedData.Num());
	CompressedData.SetNum(sizeof(FVoxelOodleHeader) + CompressedSize);

	const TVoxelArrayView<uint8> HeaderBytes = MakeVoxelArrayView(CompressedData).LeftOf(sizeof(FVoxelOodleHeader));
	FVoxelOodleHeader& Header = FromByteVoxelArrayView<FVoxelOodleHeader>(HeaderBytes);
	Header.Tag = FVoxelOodleHeader().Tag;
	Header.UncompressedSize = Data.Num();
	Header.CompressedSize = CompressedSize;

	return CompressedData;
}

bool FVoxelUtilities::Decompress(
	const TConstVoxelArrayView64<uint8> CompressedData,
	TVoxelArray64<uint8>& OutData,
	const bool bAllowParallel)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(OutData.Num() == 0);
	OutData.Reset();

	if (CompressedData.Num() == 0)
	{
		return true;
	}

	if (!ensureVoxelSlow(CompressedData.Num() >= sizeof(FVoxelOodleHeader)))
	{
		return false;
	}

	const TConstVoxelArrayView<uint8> HeaderBytes = MakeVoxelArrayView(CompressedData).LeftOf(sizeof(FVoxelOodleHeader));
	const FVoxelOodleHeader Header = FromByteVoxelArrayView<FVoxelOodleHeader>(HeaderBytes);

	if (!ensureVoxelSlow(Header.Tag == FVoxelOodleHeader().Tag) ||
		!ensureVoxelSlow(sizeof(FVoxelOodleHeader) + Header.CompressedSize == CompressedData.Num()))
	{
		return false;
	}

	using namespace FOodleDataCompression;

	TVoxelArray64<uint8> UncompressedData;
	FVoxelUtilities::SetNumFast(UncompressedData, Header.UncompressedSize);

#if VOXEL_ENGINE_VERSION >= 503
	if (bAllowParallel)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("DecompressParallel %lldB", Header.UncompressedSize);

		if (!ensure(FOodleDataCompression::DecompressParallel(
			UncompressedData.GetData(),
			Header.UncompressedSize,
			CompressedData.GetData() + sizeof(FVoxelOodleHeader),
			Header.CompressedSize)))
		{
			return false;
		}
	}
	else
#endif
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Decompress %lldB", Header.UncompressedSize);

		if (!ensure(FOodleDataCompression::Decompress(
			UncompressedData.GetData(),
			Header.UncompressedSize,
			CompressedData.GetData() + sizeof(FVoxelOodleHeader),
			Header.CompressedSize)))
		{
			return false;
		}
	}

	OutData = MoveTemp(UncompressedData);
	return true;
}