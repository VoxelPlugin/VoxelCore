// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FVoxelWriterArchive::FVoxelWriterArchive()
{
	SetIsSaving(true);
	SetIsPersistent(true);
}

void FVoxelWriterArchive::Serialize(void* Data, const int64 NumToSerialize)
{
	VOXEL_FUNCTION_COUNTER_NUM(NumToSerialize, 128);

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

int64 FVoxelWriterArchive::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelWriterArchive::GetArchiveName() const
{
	return "FVoxelArchive";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelReaderArchive::FVoxelReaderArchive(const TConstVoxelArrayView64<uint8>& Bytes): Bytes(Bytes)
{
	SetIsLoading(true);
	SetIsPersistent(true);
}

void FVoxelReaderArchive::Serialize(void* Data, const int64 NumToSerialize)
{
	VOXEL_FUNCTION_COUNTER_NUM(NumToSerialize, 128);

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

int64 FVoxelReaderArchive::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelReaderArchive::GetArchiveName() const
{
	return "FVoxelArchive";
}