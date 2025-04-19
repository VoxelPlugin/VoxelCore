// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDependency.h"
#include "VoxelInvalidationQueue.h"

// Not a FVoxelSingleton, we don't want to free the memory on shutdown to avoid crashing when other singletons tear down
class FVoxelDependencyManager
{
public:
	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	FORCEINLINE int64 GetAllocatedSize() const
	{
		return
			Dependencies_RequiresLock.GetAllocatedSize() +
			DependencyTrackers_RequiresLock.GetAllocatedSize() +
			InvalidationQueues_RequiresLock.GetAllocatedSize();
	}
	FORCEINLINE int32 GetReferencersNumChunks() const
	{
		checkVoxelSlow(DependencyTrackers_CriticalSection.IsLocked_Read());

		return FVoxelUtilities::DivideCeil_Positive(
			DependencyTrackers_RequiresLock.Max_Unsafe(),
			FVoxelChunkedBitArrayTS::ChunkSize);
	}

	void Dump() const;

public:
	FVoxelSharedCriticalSection Dependencies_CriticalSection;
	FVoxelSharedCriticalSection DependencyTrackers_CriticalSection;
	FVoxelSharedCriticalSection InvalidationQueues_CriticalSection;

public:
	FORCEINLINE FVoxelDependencyBase& GetDependency_RequiresLock(const FVoxelDependencyRef DependencyRef)
	{
		checkVoxelSlow(Dependencies_CriticalSection.IsLocked_Read());

		FVoxelDependencyBase& Dependency = Dependencies_RequiresLock[DependencyRef.Index];
		checkVoxelSlow(Dependency.DependencyRef == DependencyRef);
		return Dependency;
	}
	FORCEINLINE FVoxelDependencyBase* TryGetDependency_RequiresLock(const FVoxelDependencyRef DependencyRef)
	{
		checkVoxelSlow(Dependencies_CriticalSection.IsLocked_Read());

		if (!Dependencies_RequiresLock.IsAllocated_ValidIndex(DependencyRef.Index))
		{
			return nullptr;
		}

		FVoxelDependencyBase& Dependency = Dependencies_RequiresLock[DependencyRef.Index];
		checkVoxelSlow(Dependency.DependencyRef.Index == DependencyRef.Index);

		if (Dependency.DependencyRef.SerialNumber != DependencyRef.SerialNumber)
		{
			return nullptr;
		}

		checkVoxelSlow(Dependency.DependencyRef ==  DependencyRef);
		return &Dependency;
	}

	template<typename LambdaType>
	void ForeachDependency_RequiresLock(LambdaType Lambda) const
	{
		checkVoxelSlow(Dependencies_CriticalSection.IsLocked_Read());
		Dependencies_RequiresLock.Foreach(Lambda);
	}

public:
	FORCEINLINE FVoxelDependencyTracker& GetDependencyTracker_RequiresLock(const int32 Index)
	{
		checkVoxelSlow(DependencyTrackers_CriticalSection.IsLocked_Read());
		return *DependencyTrackers_RequiresLock[Index];
	}
	FORCEINLINE const TVoxelSparseArray<FVoxelInvalidationQueue*>& GetInvalidationQueues_RequiresLock() const
	{
		checkVoxelSlow(InvalidationQueues_CriticalSection.IsLocked_Read());
		return InvalidationQueues_RequiresLock;
	}

public:
	FVoxelDependencyBase& AllocateDependency(const FString& Name);
	void FreeDependency(const FVoxelDependencyBase& Dependency);

	FVoxelDependencyTracker& AllocateDependencyTracker();
	void FreeDependencyTracker(const FVoxelDependencyTracker& Tracker);

	int32 AddInvalidationQueue(FVoxelInvalidationQueue* InvalidationQueue);
	void RemoveInvalidationQueue(int32 Index);

private:
	TVoxelChunkedSparseArray<FVoxelDependencyBase> Dependencies_RequiresLock;
	TVoxelChunkedSparseArray<FVoxelDependencyTracker*> DependencyTrackers_RequiresLock;
	TVoxelSparseArray<FVoxelInvalidationQueue*> InvalidationQueues_RequiresLock;
};
extern FVoxelDependencyManager* GVoxelDependencyManager;