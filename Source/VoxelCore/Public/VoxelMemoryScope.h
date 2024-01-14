// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelMemoryScope
{
public:
	FVoxelMemoryScope();
	~FVoxelMemoryScope();

	void Clear();

public:
	struct FBlock
	{
		uint64 Size : 48;
		uint64 Alignment : 16;
		void* UnalignedPtr = nullptr;
	};
	checkStatic(sizeof(FBlock) == 16);

	static FBlock& GetBlock(void* Original);

	static uint64 StaticGetAllocSize(void* Original);
	static void* StaticMalloc(uint64 Count, uint32 Alignment);
	static void* StaticRealloc(void* Original, uint64 OriginalCount, uint64 Count, uint32 Alignment);
	static void StaticFree(void* Original);

public:
	void* Malloc(uint64 Count, uint32 Alignment);
	void* Realloc(void* Original, uint64 OriginalCount, uint64 Count, uint32 Alignment);
	void Free(void* Original);

private:
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	static constexpr int32 NumPools = 44;
	static constexpr int32 MaxAlignmentIndex = 4;

	struct FPool
	{
		TVoxelArray<void*, FDefaultAllocator> Allocations;
	};
	TVoxelStaticArray<TVoxelStaticArray<FPool, NumPools>, MaxAlignmentIndex> AlignmentToPools;

	FORCEINLINE static int32 GetAlignmentIndex(uint32 Alignment)
	{
		if (Alignment < 16)
		{
			Alignment = 16;
		}
		const int32 Index = FVoxelUtilities::ExactLog2(Alignment) - 4;
		checkVoxelSlow(0 <= Index && Index < MaxAlignmentIndex);
		return Index;
	}
	FORCEINLINE static int32 GetPoolIndex(const uint64 Count)
	{
		// 0 - 12
		if (Count <= 4096)
		{
			return FMath::CeilLogTwo(uint32(Count));
		}

		// 12 - 43
		if (Count <= 131072)
		{
			return 12 + FVoxelUtilities::DivideCeil_Positive(int32(Count), 4096) - 1;
		}

		return -1;
	}
	FORCEINLINE static uint64 GetPoolSize(const int32 PoolIndex)
	{
		checkVoxelSlow(PoolIndex >= 0);

		if (PoolIndex <= 12)
		{
			return uint64(1) << PoolIndex;
		}

		return uint64(PoolIndex - 12 + 1) * 4096;
	}
};