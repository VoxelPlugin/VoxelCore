// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependency.h"
#include "VoxelDependencySink.h"
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
	if (Invalidations.Num() > 0)
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

	if (IsInGameThread() && false)
	{
		// Causes race conditions in landscape pipeline

		Voxel::AsyncTask([Invalidations = MakeUniqueCopy(MoveTemp(Invalidations))]
		{
			FVoxelDependencyInvalidationScope Invalidator;

			ensure(GVoxelDependencyInvalidationScope->Invalidations.Num() == 0);
			GVoxelDependencyInvalidationScope->Invalidations = MoveTemp(*Invalidations);
		});
		return;
	}

	TVoxelSet<TWeakPtr<FVoxelDependencyTracker>> Trackers;

	const auto FlushInvalidations = [&]
	{
		VOXEL_SCOPE_COUNTER("FlushInvalidations");

		for (const FInvalidation& Invalidation : Invalidations)
		{
			Invalidation.Dependency->GetInvalidatedTrackers(Invalidation.Parameters, Trackers);
		}
		Invalidations.Reset();
	};

	FlushInvalidations();

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
		Trackers.Reset();

		// This might add new invalidation to Invalidations
		VOXEL_SCOPE_COUNTER("OnInvalidated");
		for (const TVoxelUniqueFunction<void()>& OnInvalidated : OnInvalidatedArray)
		{
			OnInvalidated();
		}

		FlushInvalidations();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependency::Invalidate(const FVoxelDependencyInvalidationParameters& Parameters)
{
	FVoxelDependencySink::AddAction(MakeStrongPtrLambda(this, [=, this]
	{
		VOXEL_FUNCTION_COUNTER();

		OnInvalidated.Broadcast(Parameters);

		FVoxelDependencyInvalidationScope LocalScope;
		FVoxelDependencyInvalidationScope& RootScope = *GVoxelDependencyInvalidationScope;

		RootScope.Invalidations.Add(FVoxelDependencyInvalidationScope::FInvalidation
		{
			AsShared(),
			Parameters
		});
	}));
}

void FVoxelDependency::Invalidate(const FVoxelBox& Bounds)
{
	Invalidate(FVoxelDependencyInvalidationParameters
	{
		Bounds
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependency::FVoxelDependency(const FString& Name)
	: Name(Name)
{
	VOXEL_FUNCTION_COUNTER();
	TrackerRefs_RequiresLock.Reserve(8192);
	UpdateStats();
}

void FVoxelDependency::GetInvalidatedTrackers(
	FVoxelDependencyInvalidationParameters Parameters,
	TVoxelSet<TWeakPtr<FVoxelDependencyTracker>>& OutTrackers)
{
	VOXEL_FUNCTION_COUNTER();

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

		OutTrackers.Add(TrackerRef.WeakTracker);
	});
}