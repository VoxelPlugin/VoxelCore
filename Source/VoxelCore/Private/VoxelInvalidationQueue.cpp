// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInvalidationQueue.h"
#include "VoxelDependencyManager.h"
#include "VoxelInvalidationCallstack.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelInvalidationQueue);

FTSSimpleMulticastDelegate Voxel::OnDependencyFlush;

TSharedRef<FVoxelInvalidationQueue> FVoxelInvalidationQueue::Create()
{
	Voxel::OnDependencyFlush.Broadcast();

	FVoxelInvalidationQueue* InvalidationQueue = new FVoxelInvalidationQueue();

	const int32 Index = GVoxelDependencyManager->AddInvalidationQueue(InvalidationQueue);

	return MakeShareable_CustomDestructor(InvalidationQueue, [=]
	{
		GVoxelDependencyManager->RemoveInvalidationQueue(Index);

		VOXEL_SCOPE_COUNTER("~FVoxelInvalidationQueue");
		delete InvalidationQueue;
	});
}

TSharedPtr<const FVoxelInvalidationCallstack> FVoxelInvalidationQueue::FindInvalidation(const FVoxelDependencyTracker& Tracker) const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_READ_LOCK(CriticalSection);

	for (const FInvalidation& Invalidation : Invalidations_RequiresLock)
	{
		if (Invalidation.ShouldInvalidate(Tracker))
		{
			TSharedRef<FVoxelInvalidationCallstack> Callstack = FVoxelInvalidationCallstack::Create("Invalidation Queue");
			Callstack->AddCaller(Invalidation.Callstack);
			return Callstack;
		}
	}

	return {};
}