// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencySnapshot.h"
#include "VoxelDependencyManager.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencySnapshot);

TSharedRef<FVoxelDependencySnapshot> FVoxelDependencySnapshot::Create()
{
	FVoxelDependencySnapshot* Snapshot = new FVoxelDependencySnapshot();

	int32 Index;
	{
		VOXEL_SCOPE_LOCK(GVoxelDependencyManager->SnapshotCriticalSection);
		Index = GVoxelDependencyManager->Snapshots_RequiresLock.Add(Snapshot);
	}

	return MakeShareable_CustomDestructor(Snapshot, [=]
	{
		{
			VOXEL_SCOPE_LOCK(GVoxelDependencyManager->SnapshotCriticalSection);
			verify(GVoxelDependencyManager->Snapshots_RequiresLock.RemoveAt_ReturnValue(Index) == Snapshot);
		}

		delete Snapshot;
	});
}

void FVoxelDependencySnapshot::InvalidateTracker(FVoxelDependencyTracker& Tracker)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_READ_LOCK(CriticalSection);

	for (const TVoxelUniqueFunction<void(FVoxelDependencyTracker&)>& AdditionalInvalidation : AdditionalInvalidations_RequiresLock)
	{
		AdditionalInvalidation(Tracker);
	}
}