// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependencyTracker;

// Used to replay invalidations that happen between when a state starts computing & when the dependency is actually added
// This is needed because the data we use to compute chunks is a snapshot of the data when the state is created
// As such, we need to make sure to add all the invalidations done after state creation
class VOXELCORE_API FVoxelDependencySnapshot
{
public:
	static TSharedRef<FVoxelDependencySnapshot> Create();

	UE_NONCOPYABLE(FVoxelDependencySnapshot);
	VOXEL_COUNT_INSTANCES();

	void InvalidateTracker(FVoxelDependencyTracker& Tracker);

private:
	FVoxelDependencySnapshot() = default;

	FVoxelSharedCriticalSection CriticalSection;
	TVoxelArray<TVoxelUniqueFunction<void(FVoxelDependencyTracker&)>> AdditionalInvalidations_RequiresLock;

	friend class FVoxelDependencyManager;
};