// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencyTracker.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyTracker);

FVoxelDependencyTracker::~FVoxelDependencyTracker()
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!IsInvalidated() || DependencyRefs.Num() == 0);
	Unregister_RequiresLock();
}

void FVoxelDependencyTracker::AddDependency(
	const TSharedRef<FVoxelDependency>& Dependency,
	const TOptional<FVoxelBox>& Bounds,
	const TOptional<uint64>& Tag)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelDependency::FTrackerRef TrackerRef;
	{
		TrackerRef.WeakTracker = AsWeak();

		if (Bounds.IsSet())
		{
			TrackerRef.bHasBounds = true;
			TrackerRef.Bounds = Bounds.GetValue();
		}

		if (Tag.IsSet())
		{
			TrackerRef.bHasTag = true;
			TrackerRef.Tag = Tag.GetValue();
		}
	}

	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!OnInvalidated);

	if (IsInvalidated())
	{
		// Invalidated while still computing
		// No need to register the dependency
		return;
	}

	TVoxelInlineArray<FVoxelDependency::FTrackerRef, 2>& TrackerRefs = DependencyToTrackerRefs.FindOrAdd(Dependency);
	if (TrackerRefs.Contains(TrackerRef))
	{
		// Already added
		return;
	}
	TrackerRefs.Add(TrackerRef);

	int32 Index;
	{
		VOXEL_SCOPE_LOCK(Dependency->CriticalSection);
		Index = Dependency->TrackerRefs_RequiresLock.Add(TrackerRef);
	}
	Dependency->UpdateStats();

	FDependencyRef& DependencyRef = DependencyRefs.Emplace_GetRef();
	DependencyRef.WeakDependency = Dependency;
	DependencyRef.Index = Index;
}

bool FVoxelDependencyTracker::TrySetOnInvalidated(TVoxelUniqueFunction<void()> NewOnInvalidated)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!OnInvalidated);

	if (IsInvalidated())
	{
		return false;
	}

	OnInvalidated = MoveTemp(NewOnInvalidated);
	return true;
}

void FVoxelDependencyTracker::SetOnInvalidated(TVoxelUniqueFunction<void()> NewOnInvalidated)
{
	CriticalSection.Lock();
	ensure(!OnInvalidated);

	if (IsInvalidated())
	{
		CriticalSection.Unlock();
		NewOnInvalidated();
		return;
	}

	OnInvalidated = MoveTemp(NewOnInvalidated);
	CriticalSection.Unlock();
}

FVoxelDependencyTracker::FVoxelDependencyTracker(const FName& Name)
	: Name(Name)
{
	DependencyRefs.Reserve(16);
}

void FVoxelDependencyTracker::Unregister_RequiresLock()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(CriticalSection.IsLocked());

	for (const FDependencyRef& DependencyRef : DependencyRefs)
	{
		const TSharedPtr<FVoxelDependency> Dependency = DependencyRef.WeakDependency.Pin();
		if (!Dependency)
		{
			continue;
		}

		VOXEL_SCOPE_LOCK(Dependency->CriticalSection);

		checkVoxelSlow(GetWeakPtrObject_Unsafe(Dependency->TrackerRefs_RequiresLock[DependencyRef.Index].WeakTracker) == this);
		Dependency->TrackerRefs_RequiresLock.RemoveAt(DependencyRef.Index);
	}
	DependencyRefs.Empty();
}