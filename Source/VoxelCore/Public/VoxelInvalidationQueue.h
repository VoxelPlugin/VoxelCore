// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependencyTracker;

// Used to replay invalidations that happen between when a state starts computing & when the dependency is actually added
// This is needed because the data we use to compute chunks is a snapshot of the data when the state is created
// As such, we need to make sure to add all the invalidations done after state creation
class VOXELCORE_API FVoxelInvalidationQueue
{
public:
	static TSharedRef<FVoxelInvalidationQueue> Create();

	UE_NONCOPYABLE(FVoxelInvalidationQueue);
	VOXEL_COUNT_INSTANCES();

	TSharedPtr<const FVoxelInvalidationCallstack> FindInvalidation(const FVoxelDependencyTracker& Tracker) const;

private:
	FVoxelInvalidationQueue() = default;

	FVoxelSharedCriticalSection CriticalSection;

	struct FInvalidation
	{
		TVoxelUniqueFunction<bool(const FVoxelDependencyTracker&)> ShouldInvalidate;
		TSharedPtr<const FVoxelInvalidationCallstack> Callstack;
	};
	TVoxelArray<FInvalidation> Invalidations_RequiresLock;

	friend class FVoxelDependencyBase;
};