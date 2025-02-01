// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependency.h"
#include "VoxelDependencySink.h"
#include "VoxelDependencyTracker.h"
#include "VoxelAABBTree.h"

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
	Invalidate(TVoxelArray<FVoxelBox>{ Bounds });
}

void FVoxelDependency::Invalidate(const TConstVoxelArrayView<FVoxelBox> Bounds)
{
	Invalidate(FVoxelDependencyInvalidationParameters
	{
		FVoxelAABBTree::Create(Bounds)
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependency::FVoxelDependency(const FString& Name)
	: Name(Name)
{
	VOXEL_FUNCTION_COUNTER();

	UpdateStats();
}

void FVoxelDependency::GetInvalidatedTrackers(
	const FVoxelDependencyInvalidationParameters& Parameters,
	TVoxelSet<TWeakPtr<FVoxelDependencyTracker>>& OutTrackers)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (TrackerRefs_RequiresLock.Num() > 0)
	{
		LOG_VOXEL(Verbose, "Invalidating %s", *Name);
	}

	TrackerRefs_RequiresLock.Foreach([&](const FTrackerRef& TrackerRef)
	{
		if (Parameters.Bounds &&
			TrackerRef.bHasBounds &&
			!Parameters.Bounds->Intersects(TrackerRef.Bounds))
		{
			return;
		}

		OutTrackers.Add(TrackerRef.WeakTracker);
	});
}