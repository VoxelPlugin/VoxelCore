// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkHash.h"

FString FVoxelBulkHash::ToString() const
{
	VOXEL_FUNCTION_COUNTER();

	FString Result;
	Result.Reserve(32);
	for (int32 Index = 0; Index < 16; Index++)
	{
		Result += FString::Printf(TEXT("%02x"), MakeByteVoxelArrayView(*this)[Index]);
	}
	return Result;
}

FVoxelBulkHash FVoxelBulkHash::Create(const TConstVoxelArrayView<uint8> Bytes)
{
	VOXEL_FUNCTION_COUNTER_NUM(Bytes.Num(), 256);

	FSHA1 Hasher;
	Hasher.Update(Bytes.GetData(), uint64(Bytes.Num()));
	Hasher.Final();

	FVoxelBulkHash Result;
	FMemory::Memcpy(&Result, Hasher.m_digest, 16);
	return Result;
}