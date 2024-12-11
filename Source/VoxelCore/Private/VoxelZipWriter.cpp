// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelZipWriter.h"

TSharedRef<FVoxelZipWriter> FVoxelZipWriter::Create(const FWriteLambda& WriteLambda)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelZipWriter> Result = MakeVoxelShareable(new(GVoxelMemory) FVoxelZipWriter(WriteLambda));
	Result->Archive.m_pIO_opaque = &Result.Get();
	Result->Archive.m_pWrite = [](void *pOpaque, const mz_uint64 file_ofs, const void *pBuf, const size_t n) -> size_t
	{
		static_cast<FVoxelZipWriter*>(pOpaque)->WriteToDisk(
			file_ofs,
			TConstVoxelArrayView64<uint8>(static_cast<const uint8*>(pBuf), n));

		return n;
	};

	ensure(mz_zip_writer_init_v2(&Result->Archive, 0, MZ_ZIP_FLAG_WRITE_ZIP64));
	Result->CheckError();
	return Result;
}

TSharedRef<FVoxelZipWriter> FVoxelZipWriter::Create(TVoxelArray64<uint8>& BulkData)
{
	return Create([&BulkData](const int64 Offset, const TConstVoxelArrayView64<uint8> Data)
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
	const bool bAllowParallel,
	const FOodleDataCompression::ECompressor Compressor,
	const FOodleDataCompression::ECompressionLevel CompressionLevel)
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::WriteCompressed_Oodle %s %lldB", *Path, Data.Num());

	const TVoxelArray64<uint8> CompressedData = FVoxelUtilities::Compress(Data, bAllowParallel, Compressor, CompressionLevel);

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
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::WriteImpl %lldB", Data.Num());

	const uint32 Crc32 = INLINE_LAMBDA
	{
		VOXEL_SCOPE_COUNTER("MemCrc32");
		return FCrc::MemCrc32(Data.GetData(), Data.Num());
	};

	// Write data outside the critical section
	struct FPendingWrite
	{
		TVoxelArray64<uint8> DataStorage;
		int64 Offset = 0;
		TConstVoxelArrayView64<uint8> Data;
	};
	TVoxelArray<FPendingWrite> PendingWrites;

	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		ensure(!WriteLambdaOverride_RequiresLock);
		WriteLambdaOverride_RequiresLock = [&](
			const int64 Offset,
			const TConstVoxelArrayView64<uint8> DataToWrite)
			{
				if (DataToWrite.GetData() == Data.GetData() &&
					DataToWrite.Num() == Data.Num())
				{
					FPendingWrite& PendingWrite = PendingWrites.Emplace_GetRef();
					PendingWrite.Offset = Offset;
					PendingWrite.Data = Data;
				}
				else if (DataToWrite.Num() < 1024)
				{
					FPendingWrite& PendingWrite = PendingWrites.Emplace_GetRef();
					PendingWrite.DataStorage = TVoxelArray64<uint8>(DataToWrite);
					PendingWrite.Offset = Offset;
					PendingWrite.Data = PendingWrite.DataStorage;
				}
				else
				{
					ensure(Compression != MZ_NO_COMPRESSION);

					// Copying the data would be too expensive
					WriteLambda(Offset, DataToWrite);
				}
			};

		ensure(mz_zip_writer_add_mem_ex(
			&Archive,
			TCHAR_TO_UTF8(*Path),
			Data.GetData(),
			Data.Num(),
			nullptr,
			0,
			Compression,
			0,
			Crc32));

		WriteLambdaOverride_RequiresLock = {};

		CheckError();
	}

	for (const FPendingWrite& PendingWrite : PendingWrites)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Write %lldB", PendingWrite.Data.Num());
		WriteLambda(PendingWrite.Offset, PendingWrite.Data);
	}
}

void FVoxelZipWriter::WriteToDisk(
	const int64 Offset,
	const TConstVoxelArrayView64<uint8> Data) const
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipWriter::WriteToDisk %lldB", Data.Num());
	checkVoxelSlow(CriticalSection.IsLocked());

	if (WriteLambdaOverride_RequiresLock)
	{
		WriteLambdaOverride_RequiresLock(Offset, Data);
		return;
	}

	WriteLambda(Offset, Data);
}