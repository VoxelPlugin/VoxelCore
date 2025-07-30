// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelAllocator, "FVoxelAllocator");

struct VOXELCORE_API FVoxelAllocation
{
public:
	const int64 Index;
	const int64 Num;
	const int64 Padding;

private:
	const int32 PoolIndex;

	FVoxelAllocation(
		const int64 Index,
		const int64 Num,
		const int64 Padding,
		const int32 PoolIndex)
		: Index(Index)
		, Num(Num)
		, Padding(Padding)
		, PoolIndex(PoolIndex)
	{
	}

	friend class FVoxelAllocator;
};

class VOXELCORE_API FVoxelAllocator
{
public:
	explicit FVoxelAllocator(int64 MaxSize);

	FVoxelAllocation Allocate(int64 Num);
	void Free(const FVoxelAllocation& Allocation);

	int64 GetMax() const
	{
		return Max.Get();
	}

private:
	struct FAllocationPool
	{
	public:
		const int64 PoolSize;

		explicit FAllocationPool(const int32 PoolIndex)
			: PoolSize(GetPoolSize(PoolIndex))
		{
		}

		int64 Allocate(FVoxelAllocator& Allocator);
		void Free(int64 Index);

	public:
		VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelAllocator);

		int64 GetAllocatedSize() const;

	private:
		FVoxelCriticalSection_NoPadding CriticalSection;
		TVoxelArray<int64> FreeIndices_RequiresLock;
	};

private:
	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelAllocator);

	int64 GetAllocatedSize() const
	{
		return PoolIndexToPool.GetAllocatedSize();
	}

	FVoxelCounter64 Max;
	TVoxelArray<FAllocationPool> PoolIndexToPool;

private:
	FORCEINLINE static int32 NumToPoolIndex(const int64 Num)
	{
		const int32 PoolIndex = INLINE_LAMBDA -> int32
		{
			// 0 <= PoolIndex <= 10
			if (Num <= 1024)
			{
				return FMath::CeilLogTwo(Num);
			}

			if (Num <= 64 * 1024)
			{
				// 11 <= PoolIndex <= 73
				return 10 + FVoxelUtilities::DivideCeil_Positive<int64>(Num, 1024) - 1;
			}

			// 74 <= PoolIndex
			// Some pools will be empty but it makes the math easier
			return 74 + FMath::CeilLogTwo64(Num);
		};
		checkVoxelSlow(PoolIndex == 0 || GetPoolSize(PoolIndex - 1) < Num);
		checkVoxelSlow(Num <= GetPoolSize(PoolIndex));

		return PoolIndex;
	}
	FORCEINLINE static int64 GetPoolSize(const int32 PoolIndex)
	{
		checkVoxelSlow(PoolIndex >= 0);

		if (PoolIndex <= 10)
		{
			return int64(1) << PoolIndex;
		}

		if (PoolIndex <= 73)
		{
			return (PoolIndex - 10 + 1) * 1024;
		}

		return int64(1) << (PoolIndex - 74);
	}
};