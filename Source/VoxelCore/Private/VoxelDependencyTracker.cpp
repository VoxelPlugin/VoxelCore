// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencyTracker.h"
#include "VoxelDependencySink.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyTracker);
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelDependencyTrackerMemory);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyTracker::~FVoxelDependencyTracker()
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!IsInvalidated() || DependencyRefs_RequiresLock.Num() == 0);
	Unregister_RequiresLock();
}

int64 FVoxelDependencyTracker::GetAllocatedSize() const
{
	return
		sizeof(*this) +
		DependencyRefs_RequiresLock.GetAllocatedSize() +
		DependencyToTrackerRefs_RequiresLock.GetAllocatedSize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependencyTracker::AddDependency(
	const TSharedRef<FVoxelDependency>& Dependency,
	const TOptional<FVoxelBox>& Bounds)
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
	}

	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!bIsFinalized_RequiresLock);

	if (IsInvalidated())
	{
		// Invalidated while still computing
		// No need to register the dependency
		return;
	}

	TVoxelInlineArray<FVoxelDependency::FTrackerRef, 2>& TrackerRefs = DependencyToTrackerRefs_RequiresLock.FindOrAdd(Dependency);
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

	FDependencyRef& DependencyRef = DependencyRefs_RequiresLock.Emplace_GetRef();
	DependencyRef.WeakDependency = Dependency;
	DependencyRef.Index = Index;
}

bool FVoxelDependencyTracker::SetOnInvalidated(
	TVoxelUniqueFunction<void()> NewOnInvalidated,
	const bool bFireNow,
	const bool bFinalize)
{
	CriticalSection.Lock();

	if (bFinalize)
	{
		checkVoxelSlow(!bIsFinalized_RequiresLock);
		bIsFinalized_RequiresLock = true;
	}

	DependencyToTrackerRefs_RequiresLock.Shrink();
	DependencyToTrackerRefs_RequiresLock.Empty();

	checkVoxelSlow(!OnInvalidated_RequiresLock);

	if (IsInvalidated())
	{
		CriticalSection.Unlock();

		if (bFireNow)
		{
			NewOnInvalidated();
		}
		return false;
	}

	OnInvalidated_RequiresLock = MoveTemp(NewOnInvalidated);
	CriticalSection.Unlock();

	return true;
}

void FVoxelDependencyTracker::Invalidate()
{
	FVoxelDependencySink::AddAction(MakeWeakPtrLambda(this, [this]
	{
		VOXEL_FUNCTION_COUNTER();

		FVoxelDependencyInvalidationScope LocalScope;
		FVoxelDependencyInvalidationScope& RootScope = FVoxelDependencyInvalidationScope::RootScope();

		RootScope.InvalidatedTrackers.Add(AsWeak());
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyTracker::FVoxelDependencyTracker(const FName& Name)
	: Name(Name)
{
	VOXEL_FUNCTION_COUNTER();

	DependencyRefs_RequiresLock.Reserve(128);
}

void FVoxelDependencyTracker::Unregister_RequiresLock()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(CriticalSection.IsLocked());

	for (const FDependencyRef& DependencyRef : DependencyRefs_RequiresLock)
	{
		const TSharedPtr<FVoxelDependency> Dependency = DependencyRef.WeakDependency.Pin();
		if (!Dependency)
		{
			continue;
		}

		VOXEL_SCOPE_LOCK(Dependency->CriticalSection);

		checkVoxelSlow(GetWeakPtrObject_Unsafe(Dependency->TrackerRefs_RequiresLock[DependencyRef.Index].WeakTracker) == this);
		Dependency->TrackerRefs_RequiresLock.RemoveAt(DependencyRef.Index);

		Dependency->UpdateStats();
	}
	DependencyRefs_RequiresLock.Empty();
}