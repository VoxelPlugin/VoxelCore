// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDependency.h"

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelDependencyTrackerMemory, "Voxel Dependency Tracker Memory");

class VOXELCORE_API FVoxelDependencyTracker : public TSharedFromThis<FVoxelDependencyTracker>
{
public:
	const FName Name;

	static TSharedRef<FVoxelDependencyTracker> Create(const FName Name)
	{
		return MakeVoxelShareable(new(GVoxelMemory) FVoxelDependencyTracker(Name));
	}
	~FVoxelDependencyTracker();
	UE_NONCOPYABLE(FVoxelDependencyTracker);

	VOXEL_COUNT_INSTANCES();
	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	FORCEINLINE bool IsInvalidated() const
	{
		return bIsInvalidated.Get();
	}

	int64 GetAllocatedSize() const;

public:
	void AddDependency(
		const TSharedRef<FVoxelDependency>& Dependency,
		const TOptional<FVoxelBox>& Bounds = {},
		const TOptional<uint64>& Tag = {});

	void AddObjectToKeepAlive(const FSharedVoidPtr& ObjectToKeepAlive);

	// Returns false if already invalidated
	bool SetOnInvalidated(
		TVoxelUniqueFunction<void()> NewOnInvalidated,
		bool bFireNow = true);

private:
	TVoxelAtomic<bool> bIsInvalidated;

	FVoxelCriticalSection_NoPadding CriticalSection;
	bool bIsFinalized_RequiresLock = false;
	TVoxelUniqueFunction<void()> OnInvalidated_RequiresLock;
	TVoxelArray<FSharedVoidPtr> ObjectsToKeepAlive_RequiresLock;
	// Temporary cache to avoid adding the same dependency multiple times
	TVoxelMap<TWeakPtr<FVoxelDependency>, TVoxelInlineArray<FVoxelDependency::FTrackerRef, 2>> DependencyToTrackerRefs_RequiresLock;

	struct FDependencyRef
	{
		TWeakPtr<FVoxelDependency> WeakDependency;
		int32 Index = -1;
	};
	TVoxelArray<FDependencyRef> DependencyRefs_RequiresLock;

	explicit FVoxelDependencyTracker(const FName& Name);

	void Unregister_RequiresLock();

	friend FVoxelDependency;
	friend FVoxelDependencyInvalidationScope;
};