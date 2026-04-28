// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependencyTracker;

namespace Voxel
{
	extern VOXELCORE_API FTSSimpleMulticastDelegate OnDependencyFlush;
}

// Used to replay invalidations that happen between when a state starts computing & when the dependency is actually added
// This is needed because the data we use to compute chunks is a snapshot of the data when the state is created
// As such, we need to make sure to add all the invalidations done after state creation
class VOXELCORE_API FVoxelInvalidationQueue
{
public:
	// bFlush: when true (default), broadcasts OnDependencyFlush before creating the queue,
	// which on the game thread drains pending stamp updates so the queue starts from a
	// consistent snapshot point. Pass false to skip the broadcast - required when calling
	// from a worker thread (the flush asserts game-thread). Skipping is safe when the queue
	// is only used to capture in-window invalidations and replay them into a tracker at
	// Finalize, since any pending stamp updates flushed later will land in the queue too.
	static TSharedRef<FVoxelInvalidationQueue> Create(bool bFlush = true);

	UE_NONCOPYABLE(FVoxelInvalidationQueue);
	VOXEL_COUNT_INSTANCES();

	TSharedPtr<const FVoxelInvalidationCallstack> FindInvalidation(const FVoxelDependencyTracker& Tracker) const;

private:
	FVoxelInvalidationQueue() = default;

	FVoxelSharedCriticalSection CriticalSection;

	struct FInvalidation
	{
		TVoxelUniqueFunction<bool(const FVoxelDependencyTracker&)> ShouldInvalidate;
		TSharedRef<const FVoxelInvalidationCallstack> Callstack;
	};
	TVoxelArray<FInvalidation> Invalidations_RequiresLock;

	friend class FVoxelDependencyBase;
};