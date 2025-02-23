// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependency;
class FVoxelDependency3D;
class FVoxelDependency2D;

DECLARE_UNIQUE_VOXEL_ID(FVoxelDependencyId);

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelDependencyTrackerMemory, "Voxel Dependency Tracker Memory");

class VOXELCORE_API FVoxelDependencyTracker
{
	struct FPrivate {};

public:
	static TSharedRef<FVoxelDependencyTracker> Create(FName Name);

	explicit FVoxelDependencyTracker(FPrivate) {}
	UE_NONCOPYABLE(FVoxelDependencyTracker);

	VOXEL_COUNT_INSTANCES();
	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	bool bIsFinalized_Debug = false;

public:
	FORCEINLINE bool IsInvalidated() const
	{
		return bIsInvalidated.Get();
	}

	int64 GetAllocatedSize() const;

public:
	void AddDependency(const FVoxelDependency& Dependency);

	void AddDependency(
		const FVoxelDependency2D& Dependency,
		const FVoxelBox2D& Bounds);

	void AddDependency(
		const FVoxelDependency3D& Dependency,
		const FVoxelBox& Bounds);

	void SetOnInvalidated(TVoxelUniqueFunction<void()> OnInvalidated);

private:
	int32 TrackerIndex = -1;
	FName PrivateName;

	FVoxelCriticalSection_NoPadding CriticalSection;

	TVoxelAtomic<bool> bIsInvalidated;
	TVoxelUniqueFunction<void()> OnInvalidated_RequiresLock;

	TVoxelSet<FVoxelDependencyId> Dependencies_RequiresLock;
	TVoxelMap<FVoxelDependencyId, FVoxelBox2D> Dependency2DToBounds_RequiresLock;
	TVoxelMap<FVoxelDependencyId, FVoxelBox> Dependency3DToBounds_RequiresLock;

	friend FVoxelDependency;
	friend FVoxelDependency2D;
	friend FVoxelDependency3D;
	friend class FVoxelDependencyManager;
};