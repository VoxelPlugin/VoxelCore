// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelZipWriter.h"

TSharedRef<FVoxelZipWriter> FVoxelZipWriter::Create(const FWriteLambda& WriteLambda)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelZipWriter> Result = MakeVoxelShareable(new(GVoxelMemory) FVoxelZipWriter(WriteLambda));
	Result->Archive.m_pIO_opaque = &Result.Get();
	Result->Archive.m_pWrite = [](void *pOpaque, const mz_uint64 file_ofs, const void *pBuf, const size_t n) -> size_t
	{
		VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter write %lldB", n);
		if (!ensure(static_cast<FVoxelZipWriter*>(pOpaque)->WriteLambda(
			file_ofs,
			TConstVoxelArrayView<uint8>(static_cast<const uint8*>(pBuf), n))))
		{
			return 0;
		}
		return n;
	};

	ensure(mz_zip_writer_init_v2(&Result->Archive, 0, MZ_ZIP_FLAG_WRITE_ZIP64));
	Result->CheckError();
	return Result;
}

TSharedRef<FVoxelZipWriter> FVoxelZipWriter::Create(TVoxelArray64<uint8>& BulkData)
{
	return Create([&BulkData](const int64 Offset, const TConstVoxelArrayView<uint8> Data)
	{
		if (BulkData.Num() < Offset + Data.Num())
		{
			FVoxelUtilities::SetNumFast(BulkData, Offset + Data.Num());
		}

		FVoxelUtilities::Memcpy(
			MakeVoxelArrayView(BulkData).Slice(Offset, Data.Num()),
			Data);

		return true;
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelZipWriter::HasFile(const FString& Path) const
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	return WrittenPaths.Contains(Path);
}

bool FVoxelZipWriter::Finalize()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	ensure(mz_zip_writer_finalize_archive(&Archive));
	CheckError();
	return !HasError();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelZipWriter::Write(
	const FString& Path,
	const TConstVoxelArrayView64<uint8> Data)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::Write %s %lldB", *Path, Data.Num());
	WriteImpl(Path, Data, MZ_NO_COMPRESSION);
}

void FVoxelZipWriter::WriteCompressed(
	const FString& Path,
	const TConstVoxelArrayView64<uint8> Data)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::WriteCompressed %s %lldB", *Path, Data.Num());
	WriteImpl(Path, Data, MZ_DEFAULT_COMPRESSION);
}

void FVoxelZipWriter::WriteCompressed(
	const FString& Path,
	const FString& String)
{
	const FTCHARToUTF8 UTF8String(*String);
	WriteCompressed(
		Path,
		MakeVoxelArrayView(
			ReinterpretCastPtr<const uint8>(UTF8String.Get()),
			UTF8String.Length()));
}

void FVoxelZipWriter::WriteCompressed_Oodle(
	const FString& Path,
	const TConstVoxelArrayView64<uint8> Data,
	const FOodleDataCompression::ECompressor Compressor,
	const FOodleDataCompression::ECompressionLevel CompressionLevel)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::WriteCompressed_Oodle %s %lldB", *Path, Data.Num());
	using namespace FOodleDataCompression;

	const int64 WorkingSizeNeeded = CompressedBufferSizeNeeded(Data.Num());

	TVoxelArray64<uint8> CompressedData;
	if (Data.Num() == 0)
	{
		CompressedData.SetNumZeroed(sizeof(FOodleHeader));
		FOodleHeader& Header = FromByteVoxelArrayView<FOodleHeader>(MakeVoxelArrayView(CompressedData));
		Header = {};
	}
	else
	{
		FVoxelUtilities::SetNumFast(CompressedData, sizeof(FOodleHeader) + WorkingSizeNeeded);

		int64 CompressedSize;
		{
			VOXEL_SCOPE_COUNTER_FORMAT("CompressParallel %lldB %s %s",
				Data.Num(),
				ECompressorToString(Compressor),
				ECompressionLevelToString(CompressionLevel));

			CompressedSize = UE_503_SWITCH(Compress, CompressParallel)(
				CompressedData.GetData() + sizeof(FOodleHeader),
				WorkingSizeNeeded,
				Data.GetData(),
				Data.Num(),
				Compressor,
				CompressionLevel);
		}

		if (!ensure(CompressedSize > 0))
		{
			RaiseError();
			return;
		}

		CompressedData.SetNum(sizeof(FOodleHeader) + CompressedSize);

		const TVoxelArrayView<uint8> HeaderBytes = MakeVoxelArrayView(CompressedData).Slice(0, sizeof(FOodleHeader));
		FOodleHeader& Header = FromByteVoxelArrayView<FOodleHeader>(HeaderBytes);
		Header = {};
		Header.UncompressedSize = Data.Num();
		Header.CompressedSize = CompressedSize;
	}

	WriteImpl(Path, CompressedData, MZ_NO_COMPRESSION);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelZipWriter::WriteImpl(
	const FString& Path,
	const TConstVoxelArrayView64<uint8> Data,
	const int32 Compression)
{
	VOXEL_SCOPE_COUNTER_FORMAT("WriteImpl %lldB", Data.Num());
	VOXEL_SCOPE_LOCK(CriticalSection);

	ensure(!WrittenPaths.Contains(Path));
	WrittenPaths.Add(Path);

	ensure(mz_zip_writer_add_mem(
		&Archive,
		TCHAR_TO_UTF8(*Path),
		Data.GetData(),
		Data.Num(),
		Compression));
	CheckError();
}