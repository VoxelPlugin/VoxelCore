// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelChunkedBitArrayTS.h"

int64 FVoxelChunkedBitArrayTS::GetAllocatedSize() const
{
	return
		Chunks.GetAllocatedSize() +
		Chunks.Num() * sizeof(FChunk);
}

void FVoxelChunkedBitArrayTS::SetNumChunks(const int32 NewNumChunks)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_WRITE_LOCK(CriticalSection);

	if (Chunks.Num() >= NewNumChunks)
	{
		return;
	}

	Chunks.Reserve(NewNumChunks);

	while (Chunks.Num() < NewNumChunks)
	{
		Chunks.Add(MakeUnique<FChunk>(ForceInit));
	}
}