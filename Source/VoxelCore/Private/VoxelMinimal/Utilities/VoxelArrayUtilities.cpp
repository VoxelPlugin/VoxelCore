// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelArrayUtilitiesImpl.ispc.generated.h"

void FVoxelUtilities::Memcpy_Convert(
	const TVoxelArrayView<double> Dest,
	const TConstVoxelArrayView<float> Src)
{
	VOXEL_FUNCTION_COUNTER_NUM(Dest.Num());
	checkVoxelSlow(Dest.Num() == Src.Num());

	if (Dest.Num() == 0)
	{
		return;
	}

	return ispc::ArrayUtilities_FloatToDouble(
		Dest.GetData(),
		Src.GetData(),
		Dest.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint8> Data, const uint8 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint8(
		Data.GetData(),
		Data.Num(),
		Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint16> Data, const uint16 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint16(
		Data.GetData(),
		Data.Num(),
		Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint32> Data, const uint32 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint32(
		Data.GetData(),
		Data.Num(),
		Value);
}

bool FVoxelUtilities::AllEqual(const TConstVoxelArrayView<uint64> Data, const uint64 Value)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (Data.Num() == 0)
	{
		return true;
	}

	return ispc::ArrayUtilities_AllEqual_uint64(
		Data.GetData(),
		Data.Num(),
		Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint16 FVoxelUtilities::GetMin(const TConstVoxelArrayView<uint16> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMin_uint16(Data.GetData(), Data.Num());
}

uint16 FVoxelUtilities::GetMax(const TConstVoxelArrayView<uint16> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMax_uint16(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetMin(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMin_float(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetMax(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetMax_float(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetAbsMin(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetAbsMin(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetAbsMax(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetAbsMax(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float FVoxelUtilities::GetAbsMinSafe(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetAbsMinSafe(Data.GetData(), Data.Num());
}

float FVoxelUtilities::GetAbsMaxSafe(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return 0.f;
	}

	return ispc::ArrayUtilities_GetAbsMaxSafe(Data.GetData(), Data.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FInt32Interval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<uint8> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_uint8(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

FInt32Interval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<uint16> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_uint16(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

FInt32Interval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<int32> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FInt32Interval(MAX_int32, -MAX_int32);
	}

	FInt32Interval Result;
	ispc::ArrayUtilities_GetMinMax_int32(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

TInterval<int64> FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<int64> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return TInterval<int64>(MAX_int64, -MAX_int64);
	}

	TInterval<int64> Result;
	ispc::ArrayUtilities_GetMinMax_int64(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

FFloatInterval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FFloatInterval(0.f, 0.f);
	}

	FFloatInterval Result;
	ispc::ArrayUtilities_GetMinMax_float(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

FDoubleInterval FVoxelUtilities::GetMinMax(const TConstVoxelArrayView<double> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FDoubleInterval(0., 0.);
	}

	FDoubleInterval Result;
	ispc::ArrayUtilities_GetMinMax_double(
		Data.GetData(),
		Data.Num(),
		&Result.Min,
		&Result.Max);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::GetMinMax(
	const TConstVoxelArrayView<FVector2f> Data,
	FVector2f& OutMin,
	FVector2f& OutMax)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	OutMin = FVector2f(ForceInit);
	OutMax = FVector2f(ForceInit);

	if (!ensure(Data.Num() > 0))
	{
		return;
	}

	ispc::ArrayUtilities_GetMinMax_float2(
		ReinterpretCastPtr<ispc::float2>(Data.GetData()),
		Data.Num(),
		&ReinterpretCastRef<ispc::float2>(OutMin),
		&ReinterpretCastRef<ispc::float2>(OutMax));
}

void FVoxelUtilities::GetMinMax(
	const TConstVoxelArrayView<FColor> Data,
	FColor& OutMin,
	FColor& OutMax)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	OutMin = FColor(ForceInit);
	OutMax = FColor(ForceInit);

	if (!ensure(Data.Num() > 0))
	{
		return;
	}

	ispc::ArrayUtilities_GetMinMax_Color(
		ReinterpretCastPtr<ispc::FColor>(Data.GetData()),
		Data.Num(),
		&ReinterpretCastRef<ispc::FColor>(OutMin),
		&ReinterpretCastRef<ispc::FColor>(OutMax));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FFloatInterval FVoxelUtilities::GetMinMaxSafe(const TConstVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

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
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (!ensure(Data.Num() > 0))
	{
		return FDoubleInterval(MAX_dbl, -MAX_dbl);
	}

	FDoubleInterval Result;
	ispc::ArrayUtilities_GetMinMaxSafe_double(Data.GetData(), Data.Num(), &Result.Min, &Result.Max);
	return Result;
}

TVoxelArray<FVoxelOctahedron> FVoxelUtilities::MakeOctahedrons(const TConstVoxelArrayView<FVector3f> Vectors)
{
	VOXEL_FUNCTION_COUNTER_NUM(Vectors.Num());

	if (Vectors.Num() == 0)
	{
		return {};
	}

	TVoxelArray<FVoxelOctahedron> Result;
	SetNumFast(Result, Vectors.Num());

	ispc::ArrayUtilities_MakeOctahedrons_AOS(
		ReinterpretCastPtr<ispc::float3>(Vectors.GetData()),
		Result.GetData(),
		Result.Num());

	return Result;
}

TVoxelArray<FVoxelOctahedron> FVoxelUtilities::MakeOctahedrons(
	const TConstVoxelArrayView<float> X,
	const TConstVoxelArrayView<float> Y,
	const TConstVoxelArrayView<float> Z)
{
	const int32 Num = X.Num();
	checkVoxelSlow(Num == Y.Num());
	checkVoxelSlow(Num == Z.Num());

	VOXEL_FUNCTION_COUNTER_NUM(Num, 1024);

	if (Num == 0)
	{
		return {};
	}

	TVoxelArray<FVoxelOctahedron> Result;
	SetNumFast(Result, Num);

	ispc::ArrayUtilities_MakeOctahedrons_SOA(
		X.GetData(),
		Y.GetData(),
		Z.GetData(),
		Result.GetData(),
		Result.Num());

	return Result;
}

void FVoxelUtilities::FixupSignBit(const TVoxelArrayView<float> Data)
{
	VOXEL_FUNCTION_COUNTER_NUM(Data.Num());

	if (Data.Num() == 0)
	{
		return;
	}

	ispc::ArrayUtilities_FixupSignBit(Data.GetData(), Data.Num());
}

int32 FVoxelUtilities::CountSetBits(
	const TConstVoxelArrayView<uint32> Words,
	const TVoxelOptional<int32> InNumBits)
{
	checkVoxelSlow(Words.Num() * int64(32) <= MAX_int32);

	const int32 NumBits = InNumBits.IsSet()
		? InNumBits.GetValue()
		: Words.Num() * 32;

	VOXEL_FUNCTION_COUNTER_NUM(NumBits);

	const int32 NumWords = DivideFloor_Positive(NumBits, 32);

	int64 Count = 0;
	{
		const TConstVoxelArrayView<uint64> Words64 = Words.LeftOf(2 * DivideFloor_Positive(Words.Num(), 2)).ReinterpretAs<uint64>();

		for (const uint64 Word64 : Words64)
		{
			Count += CountBits(Word64);
		}
	}

	if (NumWords % 2 == 1)
	{
		Count += CountBits(Words[NumWords - 1]);
	}

	const int32 NumBitsLeft = NumBits & FVoxelBitArrayUtilities::IndexInWordMask;
	if (NumBitsLeft > 0)
	{
		Count += CountBits(Words[NumWords] & ((1u << NumBitsLeft) - 1));
	}

#if VOXEL_DEBUG
	int32 DebugCount = 0;
	for (int32 Index = 0; Index < NumBits; Index++)
	{
		if (FVoxelBitArrayUtilities::Get(Words, Index))
		{
			DebugCount++;
		}
	}
	check(Count == DebugCount);
#endif

	return Count;
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
	const FVoxelOodleHeader Header = CastBytes<FVoxelOodleHeader>(HeaderBytes);

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

		CompressedSize = FOodleDataCompression::CompressParallel(
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
	FVoxelOodleHeader& Header = CastBytes<FVoxelOodleHeader>(HeaderBytes);
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
	const FVoxelOodleHeader Header = CastBytes<FVoxelOodleHeader>(HeaderBytes);

	if (!ensureVoxelSlow(Header.Tag == FVoxelOodleHeader().Tag) ||
		!ensureVoxelSlow(sizeof(FVoxelOodleHeader) + Header.CompressedSize == CompressedData.Num()))
	{
		return false;
	}

	using namespace FOodleDataCompression;

	TVoxelArray64<uint8> UncompressedData;
	FVoxelUtilities::SetNumFast(UncompressedData, Header.UncompressedSize);

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