// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FVoxelWriter::FVoxelWriter()
{
	SetIsSaving(true);
	SetIsPersistent(true);
	SetWantBinaryPropertySerialization(true);
}

void FVoxelWriter::Serialize(void* Data, const int64 NumToSerialize)
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

int64 FVoxelWriter::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelWriter::GetArchiveName() const
{
	return "FVoxelArchive";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelReader::FVoxelReader(const TConstVoxelArrayView64<uint8>& Bytes): Bytes(Bytes)
{
	SetIsLoading(true);
	SetIsPersistent(true);
	SetWantBinaryPropertySerialization(true);
}

void FVoxelReader::Serialize(void* Data, const int64 NumToSerialize)
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

int64 FVoxelReader::TotalSize()
{
	return Bytes.Num();
}

FString FVoxelReader::GetArchiveName() const
{
	return "FVoxelArchive";
}