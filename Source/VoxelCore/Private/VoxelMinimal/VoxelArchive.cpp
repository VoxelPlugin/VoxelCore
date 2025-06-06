// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FVoxelWriter::FVoxelWriter()
{
	Impl.SetIsSaving(true);
	Impl.SetIsPersistent(true);
}

void FVoxelWriter::FArchiveImpl::Serialize(void* Data, const int64 NumToSerialize)
{
	if (NumToSerialize == 0)
	{
		return;
	}

	if (Bytes.Num() < Offset + NumToSerialize)
	{
		Bytes.SetNumUninitialized(Offset + NumToSerialize);
	}

	FVoxelUtilities::Memcpy(
		MakeVoxelArrayView(Bytes).Slice(Offset, NumToSerialize),
		MakeVoxelArrayView(static_cast<const uint8*>(Data), NumToSerialize));

	Offset += NumToSerialize;
}

int64 FVoxelWriter::FArchiveImpl::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelWriter::FArchiveImpl::GetArchiveName() const
{
	return "FVoxelArchive";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelReader::FVoxelReader(const TConstVoxelArrayView64<uint8> Bytes)
	: Impl(Bytes)
{
	Impl.SetIsLoading(true);
	Impl.SetIsPersistent(true);
}

void FVoxelReader::FArchiveImpl::Serialize(void* Data, const int64 NumToSerialize)
{
	if (IsError() ||
		NumToSerialize == 0)
	{
		return;
	}

	// Only serialize if we have the requested amount of data
	if (Offset + NumToSerialize > Bytes.Num())
	{
		ensureVoxelSlow(false);
		SetError();
		return;
	}

	FVoxelUtilities::Memcpy(
		MakeVoxelArrayView(static_cast<uint8*>(Data), NumToSerialize),
		Bytes.Slice(Offset, NumToSerialize));

	Offset += NumToSerialize;
}

int64 FVoxelReader::FArchiveImpl::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelReader::FArchiveImpl::GetArchiveName() const
{
	return "FVoxelArchive";
}