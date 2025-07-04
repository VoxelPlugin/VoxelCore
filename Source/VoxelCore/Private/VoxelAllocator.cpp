// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelAllocator.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelAllocator);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelAllocator::FVoxelAllocator(const int64 MaxSize)
{
	VOXEL_FUNCTION_COUNTER();

	const int32 NumPools = NumToPoolIndex(MaxSize) + 1;

	PoolIndexToPool.Reserve(NumPools);

	for (int32 Index = 0; Index < NumPools; Index++)
	{
		PoolIndexToPool.Add(FAllocationPool(Index));
	}

	UpdateStats();
}

FVoxelAllocation FVoxelAllocator::Allocate(const int64 Num)
{
	const int32 PoolIndex = NumToPoolIndex(Num);
	const int64 Index = PoolIndexToPool[PoolIndex].Allocate(*this);

	return FVoxelAllocation(
		Index,
		Num,
		GetPoolSize(PoolIndex) - Num,
		PoolIndex);
}

void FVoxelAllocator::Free(const FVoxelAllocation& Allocation)
{
	PoolIndexToPool[Allocation.PoolIndex].Free(Allocation.Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelAllocator::FAllocationPool::Allocate(FVoxelAllocator& Allocator)
{
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (FreeIndices_RequiresLock.Num() > 0)
		{
			return FreeIndices_RequiresLock.Pop();
		}
	}

	return Allocator.Max.Add_ReturnOld(PoolSize);
}

void FVoxelAllocator::FAllocationPool::Free(const int64 Index)
{
	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		FreeIndices_RequiresLock.Add(Index);
	}

	UpdateStats();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelAllocator::FAllocationPool::GetAllocatedSize() const
{
	return FreeIndices_RequiresLock.GetAllocatedSize();
}