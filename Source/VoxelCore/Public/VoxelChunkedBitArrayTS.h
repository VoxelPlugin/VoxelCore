// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelChunkedBitArrayTS
{
public:
	static constexpr int32 ChunkSize = 32 * 1024;
	using FChunk = TVoxelStaticBitArray<ChunkSize>;

	FVoxelChunkedBitArrayTS() = default;
	UE_NONCOPYABLE(FVoxelChunkedBitArrayTS);

public:
	int64 GetAllocatedSize() const;
	void SetNumChunks(const int32 NewNumChunks);

	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32)>
	void ForAllSetBits(LambdaType Lambda) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Chunks.Num() * ChunkSize);
		VOXEL_SCOPE_READ_LOCK(CriticalSection);

		for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ChunkIndex++)
		{
			checkVoxelSlow(Chunks[ChunkIndex]);
			Chunks[ChunkIndex]->ForAllSetBits([&](const int32 ChunkOffset)
			{
				Lambda(ChunkIndex * ChunkSize + ChunkOffset);
			});
		}
	}

public:
	FORCEINLINE int32 NumBits() const
	{
		return ChunkSize * Chunks.Num();
	}

	FORCEINLINE bool Set_ReturnOld(const int32 Index, const bool bValue)
	{
		VOXEL_SCOPE_READ_LOCK(CriticalSection);

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<ChunkSize>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<ChunkSize>(Index);

		checkVoxelSlow(Chunks[ChunkIndex]);
		return Chunks[ChunkIndex]->AtomicSet_ReturnOld(ChunkOffset, bValue);
	}

	FORCEINLINE const bool operator[](const int32 Index) const
	{
		VOXEL_SCOPE_READ_LOCK(CriticalSection);

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<ChunkSize>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<ChunkSize>(Index);

		checkVoxelSlow(Chunks[ChunkIndex]);
		return (*Chunks[ChunkIndex])[ChunkOffset];
	}

private:
	FVoxelSharedCriticalSection CriticalSection;
	TVoxelArray<TUniquePtr<FChunk>> Chunks;
};