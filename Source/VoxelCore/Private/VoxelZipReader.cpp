// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelZipReader.h"

TSharedPtr<FVoxelZipReader> FVoxelZipReader::Create(
	const int64 TotalSize,
	const FReadLambda& ReadLambda)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelZipReader> Result = MakeVoxelShareable(new(GVoxelMemory) FVoxelZipReader(ReadLambda));
	Result->Archive.m_pIO_opaque = &Result.Get();
	Result->Archive.m_pRead = [](void* pOpaque, const mz_uint64 file_ofs, void* pBuf, const size_t n) -> size_t
	{
		VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipReader read %lldB", n);
		if (!ensure(static_cast<FVoxelZipReader*>(pOpaque)->ReadLambda(
			file_ofs,
			TVoxelArrayView64<uint8>(static_cast<uint8*>(pBuf), n))))
		{
			return 0;
		}
		return n;
	};

	ensure(mz_zip_reader_init(&Result->Archive, TotalSize, 0));
	Result->CheckError();

	if (Result->HasError())
	{
		return nullptr;
	}

	for (int32 Index = 0; Index < int64(Result->Archive.m_total_files); Index++)
	{
		const uint32 Size = mz_zip_reader_get_filename(
			&Result->Archive,
			Index,
			nullptr,
			0);
		Result->CheckError();

		if (!ensure(Size))
		{
			continue;
		}

		TVoxelArray<char> UTF8String;
		UTF8String.SetNumZeroed(Size);

		if (!ensure(mz_zip_reader_get_filename(
			&Result->Archive,
			Index,
			UTF8String.GetData(),
			UTF8String.Num()) == UTF8String.Num()))
		{
			Result->CheckError();
			continue;
		}
		Result->CheckError();

		FString Path(UTF8String);
		Path.TrimToNullTerminator();

		ensure(Result->IndexToPath.Add(Path) == Index);

		if (ensure(!Result->PathToIndex.Contains(Path)))
		{
			Result->PathToIndex.Add_CheckNew(Path, Index);
		}
	}

	if (Result->HasError())
	{
		return nullptr;
	}

	return Result;
}

TSharedPtr<FVoxelZipReader> FVoxelZipReader::Create(const TConstVoxelArrayView64<uint8> BulkData)
{
	return Create(BulkData.Num(), [=](const int64 Offset, const TVoxelArrayView64<uint8> OutData)
	{
		if (!ensure(BulkData.IsValidSlice(Offset, OutData.Num())))
		{
			return false;
		}

		FVoxelUtilities::Memcpy(
			OutData,
			BulkData.Slice(Offset, OutData.Num()));
		return true;
	});
}

bool FVoxelZipReader::TryLoad(
	const FString& Path,
	TVoxelArray64<uint8>& OutData,
	const bool bAllowParallel,
	int64* OutCompressedSize) const
{
	VOXEL_SCOPE_COUNTER_FORMAT("FVoxelZipReader::TryLoad %s", *Path);

	const int32* IndexPtr = PathToIndex.Find(Path);
	if (!ensure(IndexPtr))
	{
		return false;
	}

	mz_zip_archive_file_stat FileStat;
	if (!ensure(mz_zip_reader_file_stat(
		&Archive,
		*IndexPtr,
		&FileStat)))
	{
		CheckError();
		return false;
	}

	if (OutCompressedSize)
	{
		*OutCompressedSize = FileStat.m_comp_size;
	}

	FVoxelUtilities::SetNumFast(OutData, FileStat.m_uncomp_size);

	if (!ensure(mz_zip_reader_extract_to_mem_no_alloc(
		&Archive,
		*IndexPtr,
		OutData.GetData(),
		OutData.Num(),
		0,
		nullptr,
		0)))
	{
		CheckError();
		return false;
	}
	CheckError();

	if (!FVoxelUtilities::IsCompressedData(OutData))
	{
		return true;
	}

	TVoxelArray64<uint8> UncompressedData;
	if (!ensure(FVoxelUtilities::Decompress(
		OutData,
		UncompressedData,
		bAllowParallel)))
	{
		return false;
	}

	OutData = MoveTemp(UncompressedData);
	return true;
}