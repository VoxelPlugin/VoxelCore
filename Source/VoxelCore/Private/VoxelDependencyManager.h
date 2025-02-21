// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDependency.h"
#include "VoxelDependencyTracker.h"
#include "VoxelDependencySnapshot.h"

// Not a FVoxelSingleton, we don't want to free the memory on shutdown to avoid crashing when other singletons tear down
class FVoxelDependencyManager
{
public:
	mutable FVoxelCriticalSection CriticalSection;
	// Inline allocations to reduce cache misses when invalidating
	TVoxelChunkedSparseArray<FVoxelDependencyTracker> Trackers_RequiresLock;

public:
	mutable FVoxelCriticalSection SnapshotCriticalSection;
	TVoxelSparseArray<FVoxelDependencySnapshot*> Snapshots_RequiresLock;

public:
	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	int64 GetAllocatedSize() const
	{
		return Trackers_RequiresLock.GetAllocatedSize();
	}

public:
	template<typename LambdaType>
	void InvalidateTrackers(
		FVoxelDependencyBase* Dependency,
		LambdaType ShouldInvalidate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Trackers_RequiresLock.Num(), 0);

		{
			VOXEL_SCOPE_COUNTER("Snapshots");
			VOXEL_SCOPE_LOCK(SnapshotCriticalSection);

			for (FVoxelDependencySnapshot* Snapshot : Snapshots_RequiresLock)
			{
				VOXEL_SCOPE_WRITE_LOCK(Snapshot->CriticalSection);

				Snapshot->AdditionalInvalidations_RequiresLock.Add(MakeStrongPtrLambda(Dependency, [ShouldInvalidate](FVoxelDependencyTracker& Tracker)
				{
					if (Tracker.bIsInvalidated.Get())
					{
						return;
					}

					VOXEL_SCOPE_LOCK(Tracker.CriticalSection);

					if (!ShouldInvalidate(Tracker))
					{
						return;
					}

					Tracker.bIsInvalidated.Set(true);

					Tracker.Dependencies_RequiresLock.Empty();
					Tracker.Dependency2DToBounds_RequiresLock.Empty();
					Tracker.Dependency3DToBounds_RequiresLock.Empty();

					if (Tracker.OnInvalidated_RequiresLock)
					{
						Tracker.OnInvalidated_RequiresLock();
					}
				}));
			}
		}

		int32 NumTrackersInvalidated = 0;
		TVoxelChunkedArray<TVoxelUniqueFunction<void()>> OnInvalidatedArray;

		const double StartTime = FPlatformTime::Seconds();
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			Trackers_RequiresLock.Foreach([&](FVoxelDependencyTracker& Tracker)
			{
				if (Tracker.bIsInvalidated.Get())
				{
					return;
				}

				VOXEL_SCOPE_LOCK(Tracker.CriticalSection);

				if (!ShouldInvalidate(Tracker))
				{
					return;
				}

				NumTrackersInvalidated++;

				Tracker.bIsInvalidated.Set(true);

				Tracker.Dependencies_RequiresLock.Empty();
				Tracker.Dependency2DToBounds_RequiresLock.Empty();
				Tracker.Dependency3DToBounds_RequiresLock.Empty();

				if (Tracker.OnInvalidated_RequiresLock)
				{
					OnInvalidatedArray.Add(MoveTemp(Tracker.OnInvalidated_RequiresLock));
				}
			});
		}
		const double EndTime = FPlatformTime::Seconds();

		LOG_VOXEL(Verbose, "Invalidating took %-8s, %-4d trackers invalidated (out of %d trackers). Dependency: %s",
			*FVoxelUtilities::SecondsToString(EndTime - StartTime),
			NumTrackersInvalidated,
			Trackers_RequiresLock.Num(),
			*Dependency->Name);

		for (const TVoxelUniqueFunction<void()>& OnInvalidated : OnInvalidatedArray)
		{
			OnInvalidated();
		}
	}
};
extern FVoxelDependencyManager* GVoxelDependencyManager;