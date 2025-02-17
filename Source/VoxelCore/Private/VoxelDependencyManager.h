// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDependencyTracker.h"

class FVoxelDependencyManager : public FVoxelSingleton
{
public:
	mutable FVoxelCriticalSection CriticalSection;
	// Inline allocations to reduce cache misses when invalidating
	TVoxelChunkedSparseArray<FVoxelDependencyTracker> Trackers_RequiresLock;

	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	int64 GetAllocatedSize() const
	{
		return Trackers_RequiresLock.GetAllocatedSize();
	}

public:
	template<typename LambdaType>
	void InvalidateTrackers(
		const FString& Name,
		LambdaType ShouldInvalidate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Trackers_RequiresLock.Num(), 0);

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
			*Name);

		for (const TVoxelUniqueFunction<void()>& OnInvalidated : OnInvalidatedArray)
		{
			OnInvalidated();
		}
	}
};
extern FVoxelDependencyManager* GVoxelDependencyManager;