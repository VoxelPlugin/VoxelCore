// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencyTracker.h"
#include "VoxelDependencyManager.h"
#include "VoxelDependency.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyTracker);
DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelDependencyTrackerMemory);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependencyTracker> FVoxelDependencyTracker::Create()
{
	VOXEL_SCOPE_LOCK(GVoxelDependencyManager->CriticalSection);

	const int32 NewTrackerIndex = GVoxelDependencyManager->Trackers_RequiresLock.Emplace(FPrivate());
	GVoxelDependencyManager->UpdateStats();

	FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->Trackers_RequiresLock[NewTrackerIndex];
	Tracker.TrackerIndex = NewTrackerIndex;

	return MakeShareable_CustomDestructor(&Tracker, [&Tracker]
	{
		VOXEL_SCOPE_LOCK(GVoxelDependencyManager->CriticalSection);
		GVoxelDependencyManager->Trackers_RequiresLock.RemoveAt(Tracker.TrackerIndex);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelDependencyTracker::GetAllocatedSize() const
{
	return
		sizeof(*this) +
		Dependencies_RequiresLock.GetAllocatedSize() +
		Dependency2DToBounds_RequiresLock.GetAllocatedSize() +
		Dependency3DToBounds_RequiresLock.GetAllocatedSize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependencyTracker::AddDependency(const FVoxelDependency& Dependency)
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	Dependencies_RequiresLock.Add(Dependency.DependencyId);
}

void FVoxelDependencyTracker::AddDependency(
	const FVoxelDependency2D& Dependency,
	const FVoxelBox2D& Bounds)
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (FVoxelBox2D* ExistingBounds = Dependency2DToBounds_RequiresLock.Find(Dependency.DependencyId))
	{
		*ExistingBounds = ExistingBounds->UnionWith(Bounds);
		return;
	}

	Dependency2DToBounds_RequiresLock.Add_CheckNew(Dependency.DependencyId, Bounds);
}

void FVoxelDependencyTracker::AddDependency(
	const FVoxelDependency3D& Dependency,
	const FVoxelBox& Bounds)
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	if (FVoxelBox* ExistingBounds = Dependency3DToBounds_RequiresLock.Find(Dependency.DependencyId))
	{
		*ExistingBounds = ExistingBounds->UnionWith(Bounds);
		return;
	}

	Dependency3DToBounds_RequiresLock.Add_CheckNew(Dependency.DependencyId, Bounds);
}

void FVoxelDependencyTracker::SetOnInvalidated(TVoxelUniqueFunction<void()> OnInvalidated)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);
	ensure(!OnInvalidated_RequiresLock);

	if (bIsInvalidated.Get())
	{
		OnInvalidated();
		return;
	}

	OnInvalidated_RequiresLock = MoveTemp(OnInvalidated);

	// Try to save memory as we likely won't have any further dependencies added
	Dependencies_RequiresLock.Shrink();
	Dependency2DToBounds_RequiresLock.Shrink();
	Dependency3DToBounds_RequiresLock.Shrink();

	UpdateStats();
}