// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependency.h"
#include "VoxelDependencyTracker.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelDependencies);

thread_local FVoxelDependencyInvalidationScope* GVoxelDependencyInvalidationScope = nullptr;

FVoxelDependencyInvalidationScope::FVoxelDependencyInvalidationScope()
{
	if (!GVoxelDependencyInvalidationScope)
	{
		GVoxelDependencyInvalidationScope = this;
	}
}

FVoxelDependencyInvalidationScope::~FVoxelDependencyInvalidationScope()
{
	if (Trackers.Num() > 0)
	{
		Invalidate();
	}

	if (GVoxelDependencyInvalidationScope == this)
	{
		GVoxelDependencyInvalidationScope = nullptr;
	}
}

void FVoxelDependencyInvalidationScope::Invalidate()
{
	VOXEL_FUNCTION_COUNTER();
	ensure(GVoxelDependencyInvalidationScope == this);

	if (IsInGameThread())
	{
		AsyncBackgroundTask([Trackers = MakeUniqueCopy(MoveTemp(Trackers))]
		{
			FVoxelDependencyInvalidationScope Invalidator;

			ensure(GVoxelDependencyInvalidationScope->Trackers.Num() == 0);
			GVoxelDependencyInvalidationScope->Trackers = MoveTemp(*Trackers);
		});
		return;
	}

	while (Trackers.Num() > 0)
	{
		TVoxelArray<TVoxelUniqueFunction<void()>> OnInvalidatedArray;
		OnInvalidatedArray.Reserve(Trackers.Num());

		for (const TWeakPtr<FVoxelDependencyTracker>& WeakTracker : Trackers)
		{
			const TSharedPtr<FVoxelDependencyTracker> Tracker = WeakTracker.Pin();
			if (!Tracker ||
				Tracker->IsInvalidated())
			{
				// Skip lock
				continue;
			}

			VOXEL_SCOPE_LOCK(Tracker->CriticalSection);

			if (Tracker->IsInvalidated())
			{
				continue;
			}

			Tracker->bIsInvalidated.Set(true);
			Tracker->Unregister_RequiresLock();

			if (Tracker->OnInvalidated_RequiresLock)
			{
				OnInvalidatedArray.Add(MoveTemp(Tracker->OnInvalidated_RequiresLock));
			}
		}
		Trackers.Empty();

		// This might add new trackers to Trackers
		VOXEL_SCOPE_COUNTER("OnInvalidated");
		for (const TVoxelUniqueFunction<void()>& OnInvalidated : OnInvalidatedArray)
		{
			OnInvalidated();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependency::Invalidate(const FInvalidationParameters Parameters)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelDependencyInvalidationScope LocalScope;
	FVoxelDependencyInvalidationScope& RootScope = *GVoxelDependencyInvalidationScope;

	const bool bCheckBounds = Parameters.Bounds.IsSet();
	const FVoxelBox Bounds = Parameters.Bounds.Get({});

	const bool bCheckTag = Parameters.LessOrEqualTag.IsSet();
	const uint64 LessOrEqualTag = Parameters.LessOrEqualTag.Get({});

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (TrackerRefs_RequiresLock.Num() > 0)
	{
		if (Parameters.Bounds.IsSet())
		{
			LOG_VOXEL(Verbose, "Invalidating %s Bounds=%s", *Name, *Parameters.Bounds->ToString());
		}
		else
		{
			LOG_VOXEL(Verbose, "Invalidating %s", *Name);
		}
	}

	TrackerRefs_RequiresLock.Foreach([&](const FTrackerRef& TrackerRef)
	{
		if (bCheckBounds &&
			TrackerRef.bHasBounds &&
			!Bounds.Intersect(TrackerRef.Bounds))
		{
			return;
		}

		if (bCheckTag &&
			TrackerRef.bHasTag &&
			!(LessOrEqualTag <= TrackerRef.Tag))
		{
			return;
		}

		RootScope.Trackers.Add(TrackerRef.WeakTracker);
	});
}

void FVoxelDependency::Invalidate(const FVoxelBox& Bounds)
{
	Invalidate(FInvalidationParameters
	{
		Bounds
	});
}

FVoxelDependency::FVoxelDependency(const FString& Name)
	: Name(Name)
{
	VOXEL_FUNCTION_COUNTER();
	TrackerRefs_RequiresLock.Reserve(8192);
	UpdateStats();
}