// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependency.h"
#include "VoxelDependencyManager.h"
#include "VoxelInvalidationCallstack.h"
#include "VoxelAABBTree.h"
#include "VoxelAABBTree2D.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelLogInvalidations, false,
	"voxel.LogInvalidations",
	"Log whenever an invalidation happen. Useful to track what's causing the voxel terrain to refresh.");

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyBase);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Coalesces Invalidate calls issued from within InvalidateTrackers to avoid very expensive behavior
struct FVoxelInvalidationBatch
{
	TVoxelMap<TWeakPtr<FVoxelDependency3D>, TVoxelChunkedArray<FVoxelBox>> Dependency3DToBoundsToInvalidate;
	TVoxelMap<TWeakPtr<FVoxelDependency2D>, TVoxelChunkedArray<FVoxelBox2D>> Dependency2DToBoundsToInvalidate;
	TVoxelSet<TWeakPtr<FVoxelDependency>> DependencyToInvalidate;
};

thread_local FVoxelInvalidationBatch* GVoxelInvalidationBatch = nullptr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelDependencyBase::GetAllocatedSize() const
{
	return ReferencingTrackers.GetAllocatedSize();
}

TSharedRef<FVoxelDependencyBase> FVoxelDependencyBase::CreateImpl(const FString& Name)
{
	FVoxelDependencyBase& Dependency = GVoxelDependencyManager->AllocateDependency(Name);

	return MakeShareable_CustomDestructor(&Dependency, [&Dependency]
	{
		GVoxelDependencyManager->FreeDependency(Dependency);
	});
}

template<typename LambdaType>
void FVoxelDependencyBase::InvalidateTrackers(LambdaType ShouldInvalidate)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelInvalidationCallstack> Callstack = FVoxelInvalidationCallstack::Create(Name);

	{
		VOXEL_SCOPE_COUNTER("InvalidationQueues");
		VOXEL_SCOPE_READ_LOCK(GVoxelDependencyManager->InvalidationQueues_CriticalSection);

		for (FVoxelInvalidationQueue* InvalidationQueue : GVoxelDependencyManager->GetInvalidationQueues_RequiresLock())
		{
			VOXEL_SCOPE_WRITE_LOCK(InvalidationQueue->CriticalSection);

			InvalidationQueue->Invalidations_RequiresLock.Add(FVoxelInvalidationQueue::FInvalidation
			{
				MakeStrongPtrLambda(this, [this, ShouldInvalidate](const FVoxelDependencyTracker& Tracker)
				{
					// ShouldInvalidate can only be called if the tracker is actually referencing us
					// InvalidationQueue doesn't check that, so manually check for references here
					return
						Tracker.AllDependencies.Contains(DependencyRef) &&
						ShouldInvalidate(Tracker);
				}),
				Callstack
			});
		}
	}

	int32 NumBits;
	int32 NumTrackersChecked;
	int32 NumTrackersInvalidated;
	TVoxelMap<FMinimalName, int32> CheckedTrackerNameToCount;
	TVoxelMap<FMinimalName, int32> InvalidatedTrackerNameToCount;
	TVoxelChunkedArray<FVoxelOnInvalidated> OnInvalidatedQueue;

	const double StartTime = FPlatformTime::Seconds();
	{
		// Also lock when iterating ReferencingTrackers, otherwise the dependency tracker might be deleted in-between
		VOXEL_SCOPE_READ_LOCK(GVoxelDependencyManager->DependencyTrackers_CriticalSection);

		TVoxelChunkedArray<int32> TrackerIndices;
		ReferencingTrackers.ForAllSetBits([&](const int32 TrackerIndex)
		{
			TrackerIndices.Add(TrackerIndex);
		});

		NumBits = ReferencingTrackers.NumBits();
		NumTrackersChecked = TrackerIndices.Num();

		if (GVoxelLogInvalidations)
		{
			for (const int32 TrackerIndex : TrackerIndices)
			{
				FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->GetDependencyTracker_RequiresLock(TrackerIndex);
				CheckedTrackerNameToCount.FindOrAdd(Tracker.Name)++;
			}
		}

		TVoxelChunkedArray<int32> TrackerToInvalidate;
		{
			VOXEL_SCOPE_COUNTER_NUM("Find invalidated trackers", TrackerIndices.Num());

			if (TrackerIndices.Num() > 512 &&
				IsInGameThread())
			{
				const FVoxelCriticalSection TrackerToInvalidate_CriticalSection;

				Voxel::ParallelFor(TrackerIndices, [&](const int32 TrackerIndex)
				{
					FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->GetDependencyTracker_RequiresLock(TrackerIndex);
					checkVoxelSlow(Tracker.AllDependencies.Contains(DependencyRef));

					if (ShouldInvalidate(Tracker))
					{
						VOXEL_SCOPE_LOCK(TrackerToInvalidate_CriticalSection);
						TrackerToInvalidate.Add(TrackerIndex);
					}
				});
			}
			else
			{
				for (const int32 TrackerIndex : TrackerIndices)
				{
					FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->GetDependencyTracker_RequiresLock(TrackerIndex);
					checkVoxelSlow(Tracker.AllDependencies.Contains(DependencyRef));

					if (ShouldInvalidate(Tracker))
					{
						TrackerToInvalidate.Add(TrackerIndex);
					}
				}
			}
		}

		NumTrackersInvalidated = TrackerToInvalidate.Num();

		VOXEL_SCOPE_COUNTER_NUM("Invalidate trackers", TrackerToInvalidate.Num());

		for (const int32 TrackerIndex : TrackerToInvalidate)
		{
			FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->GetDependencyTracker_RequiresLock(TrackerIndex);

			if (FVoxelOnInvalidated OnInvalidated = Tracker.Invalidate())
			{
				OnInvalidatedQueue.Add(MoveTemp(OnInvalidated));
			}
		}

		if (GVoxelLogInvalidations)
		{
			for (const int32 TrackerIndex : TrackerToInvalidate)
			{
				FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->GetDependencyTracker_RequiresLock(TrackerIndex);
				InvalidatedTrackerNameToCount.FindOrAdd(Tracker.Name)++;
			}
		}
	}
	const double EndTime = FPlatformTime::Seconds();

	{
		VOXEL_SCOPE_COUNTER("OnInvalidatedQueue");

		// Coalesce nested Invalidate calls made by callbacks
		// Only the outermost InvalidateTrackers drains; nested ones append to the same batch
		const bool bIsOutermost = GVoxelInvalidationBatch == nullptr;
		FVoxelInvalidationBatch LocalBatch;
		if (bIsOutermost)
		{
			check(GVoxelInvalidationBatch == nullptr);
			GVoxelInvalidationBatch = &LocalBatch;
		}

		for (const FVoxelOnInvalidated& OnInvalidated : OnInvalidatedQueue)
		{
			OnInvalidated(*Callstack);
		}

		if (bIsOutermost)
		{
			// Clear TLS during drain so the issued Invalidate calls execute normally
			// and set up their own batch scope around their own OnInvalidatedQueue
			check(GVoxelInvalidationBatch == &LocalBatch);
			GVoxelInvalidationBatch = nullptr;

			VOXEL_SCOPE_COUNTER("InvalidationBatch drain");

			for (auto& It : LocalBatch.Dependency3DToBoundsToInvalidate)
			{
				if (const TSharedPtr<FVoxelDependency3D> Dependency = It.Key.Pin())
				{
					Dependency->Invalidate(FVoxelAABBTree::Create(It.Value));
				}
			}
			for (auto& It : LocalBatch.Dependency2DToBoundsToInvalidate)
			{
				if (const TSharedPtr<FVoxelDependency2D> Dependency = It.Key.Pin())
				{
					Dependency->Invalidate(FVoxelAABBTree2D::Create(It.Value));
				}
			}
			for (const TWeakPtr<FVoxelDependency>& WeakDependency : LocalBatch.DependencyToInvalidate)
			{
				if (const TSharedPtr<FVoxelDependency> Dependency = WeakDependency.Pin())
				{
					Dependency->Invalidate();
				}
			}
		}
	}

#if !NO_LOGGING
	if (!GVoxelLogInvalidations)
	{
		return;
	}

	if (NumTrackersInvalidated == 0 &&
		LogVoxel.GetVerbosity() < ELogVerbosity::Verbose)
	{
		return;
	}

	CheckedTrackerNameToCount.ValueSort([](const int32 A, const int32 B)
	{
		return A > B;
	});

	InvalidatedTrackerNameToCount.ValueSort([](const int32 A, const int32 B)
	{
		return A > B;
	});

	FString CheckedTrackerNames;
	for (const auto& It : CheckedTrackerNameToCount)
	{
		CheckedTrackerNames += FString::Printf(TEXT("\n\t\t%s x%d"), *FName(It.Key).ToString(), It.Value);
	}

	FString InvalidatedTrackerNames;
	for (const auto& It : InvalidatedTrackerNameToCount)
	{
		InvalidatedTrackerNames += FString::Printf(TEXT("\n\t\t%s x%d"), *FName(It.Key).ToString(), It.Value);
	}

	Voxel::GameTask(MakeStrongPtrLambda(this, [=, this]
	{
		LOG_VOXEL(Log,
			"Invalidating took %s, %d trackers invalidated (out of %d trackers, %d bits)"
			"\n\tDependency: %s"
			"\n\tChecked trackers: %s"
			"\n\tInvalidated trackers: %s"
			"\n\tInvalidation callstack: %s",
			*FVoxelUtilities::SecondsToString(EndTime - StartTime),
			NumTrackersInvalidated,
			NumTrackersChecked,
			NumBits,
			*Name,
			*CheckedTrackerNames,
			*InvalidatedTrackerNames,
			*Callstack->ToString().Replace(TEXT("\n"), TEXT("\n\t\t")));
	}));
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependency> FVoxelDependency::Create(const FString& Name)
{
	return StaticCastSharedRef<FVoxelDependency>(CreateImpl(Name));
}

void FVoxelDependency::Invalidate()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Invalidate %s", *Name);

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		Batch->DependencyToInvalidate.Add(WeakFromThis(*this));
		return;
	}

	InvalidateTrackers([&](const FVoxelDependencyTracker& Tracker)
	{
		checkVoxelSlow(Tracker.Dependencies.Contains(DependencyRef));
		return true;
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependency2D> FVoxelDependency2D::Create(const FString& Name)
{
	return StaticCastSharedRef<FVoxelDependency2D>(CreateImpl(Name));
}

void FVoxelDependency2D::Invalidate(const FVoxelBox2D& Bounds)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Invalidate %s", *Name);

	if (!ensureVoxelSlow(Bounds.IsValidAndNotEmpty()))
	{
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		Batch->Dependency2DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this)).Add(Bounds);
		return;
	}

	InvalidateTrackers([=, this](const FVoxelDependencyTracker& Tracker)
	{
		const int32 Index = Tracker.Dependencies_2D.Find(DependencyRef);
		checkVoxelSlow(Index != -1);
		return Bounds.Intersects(Tracker.Bounds_2D[Index]);
	});
}

void FVoxelDependency2D::Invalidate(const TConstVoxelArrayView<FVoxelBox2D> BoundsArray)
{
	VOXEL_FUNCTION_COUNTER();

	if (BoundsArray.Num() == 0)
	{
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		Batch->Dependency2DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this)).Append(BoundsArray);
		return;
	}

	Invalidate(FVoxelAABBTree2D::Create(BoundsArray));
}

void FVoxelDependency2D::Invalidate(const TSharedRef<const FVoxelAABBTree2D>& Tree)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Invalidate %s", *Name);

	if (Tree->IsEmpty())
	{
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		TVoxelChunkedArray<FVoxelBox2D>& Pending = Batch->Dependency2DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this));
		for (const FVoxelAABBTree2D::FLeaf& Leaf : Tree->GetLeaves())
		{
			for (const FVoxelAABBTree2D::FElement& Element : Leaf.Elements)
			{
				Pending.Add(Element.Bounds);
			}
		}
		return;
	}

	InvalidateTrackers([=, this](const FVoxelDependencyTracker& Tracker)
	{
		const int32 Index = Tracker.Dependencies_2D.Find(DependencyRef);
		checkVoxelSlow(Index != -1);
		return Tree->Intersects(Tracker.Bounds_2D[Index]);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependency3D> FVoxelDependency3D::Create(const FString& Name)
{
	return StaticCastSharedRef<FVoxelDependency3D>(CreateImpl(Name));
}

void FVoxelDependency3D::Invalidate(const FVoxelBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Invalidate %s", *Name);

	if (!ensureVoxelSlow(Bounds.IsValidAndNotEmpty()))
	{
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		Batch->Dependency3DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this)).Add(Bounds);
		return;
	}

	const FVoxelFastBox FastBounds(Bounds);

	InvalidateTrackers([=, this](const FVoxelDependencyTracker& Tracker)
	{
		const int32 Index = Tracker.Dependencies_3D.Find(DependencyRef);
		checkVoxelSlow(Index != -1);
		return FastBounds.Intersects(Tracker.Bounds_3D[Index]);
	});
}

void FVoxelDependency3D::Invalidate(const TConstVoxelArrayView<FVoxelBox> BoundsArray)
{
	VOXEL_FUNCTION_COUNTER();

	if (BoundsArray.Num() == 0)
	{
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		Batch->Dependency3DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this)).Append(BoundsArray);
		return;
	}

	Invalidate(FVoxelAABBTree::Create(BoundsArray));
}

void FVoxelDependency3D::Invalidate(const TSharedRef<const FVoxelAABBTree>& Tree)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Invalidate %s Tree.Num = %d", *Name, Tree->Num());

	if (Tree->IsEmpty())
	{
		return;
	}

	if (Tree->Num() == 1)
	{
		// Fast path
		Invalidate(Tree->GetBounds(0).GetBox());
		return;
	}

	if (FVoxelInvalidationBatch* Batch = GVoxelInvalidationBatch)
	{
		TVoxelChunkedArray<FVoxelBox>& Pending = Batch->Dependency3DToBoundsToInvalidate.FindOrAdd(WeakFromThis(*this));
		for (int32 Index = 0; Index < Tree->Num(); Index++)
		{
			Pending.Add(Tree->GetBounds(Index).GetBox());
		}
		return;
	}

	InvalidateTrackers([=, this](const FVoxelDependencyTracker& Tracker)
	{
		const int32 Index = Tracker.Dependencies_3D.Find(DependencyRef);
		checkVoxelSlow(Index != -1);
		return Tree->Intersects(Tracker.Bounds_3D[Index]);
	});
}