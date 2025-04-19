// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelDependency.h"
#include "VoxelDependencyManager.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelDependencyTrackerMemory);
DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyTracker);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyTracker::~FVoxelDependencyTracker()
{
	check(!IsRegisteredToDependencies.Get());
}

int64 FVoxelDependencyTracker::GetAllocatedSize() const
{
	return
		sizeof(FVoxelDependencyTracker) +
		AllDependencies.GetAllocatedSize() +
		Bounds_2D.GetAllocatedSize() +
		Bounds_3D.GetAllocatedSize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependencyTracker::RegisterToDependencies()
{
	checkVoxelSlow(!IsRegisteredToDependencies.Get());
	IsRegisteredToDependencies.Set(true);

	VOXEL_SCOPE_READ_LOCK(GVoxelDependencyManager->Dependencies_CriticalSection);

#if VOXEL_DEBUG
	GVoxelDependencyManager->ForeachDependency_RequiresLock([&](const FVoxelDependencyBase& Dependency)
	{
		check(!Dependency.ReferencingTrackers[TrackerIndex]);
	});
#endif

	for (const FVoxelDependencyRef& DependencyRef : AllDependencies)
	{
		// FVoxelDependencyCollector::SharedDependencies should keep it alive
		FVoxelDependencyBase& Dependency = GVoxelDependencyManager->GetDependency_RequiresLock(DependencyRef);
		checkVoxelSlow(Dependency.DependencyRef == DependencyRef);
		ensure(Dependency.ReferencingTrackers.Set_ReturnOld(TrackerIndex, true) == false);
	}
}

void FVoxelDependencyTracker::UnregisterFromDependencies()
{
#if VOXEL_DEBUG
	ON_SCOPE_EXIT
	{
		VOXEL_SCOPE_READ_LOCK(GVoxelDependencyManager->Dependencies_CriticalSection);

		GVoxelDependencyManager->ForeachDependency_RequiresLock([&](const FVoxelDependencyBase& Dependency)
		{
			check(!Dependency.ReferencingTrackers[TrackerIndex]);
		});
	};
#endif

	if (IsRegisteredToDependencies.Set_ReturnOld(false) == false)
	{
		// Already unregistered
		ensureVoxelSlow(IsInvalidated());
		return;
	}

	VOXEL_SCOPE_READ_LOCK(GVoxelDependencyManager->Dependencies_CriticalSection);

	for (const FVoxelDependencyRef& DependencyRef : AllDependencies)
	{
		FVoxelDependencyBase* Dependency = GVoxelDependencyManager->TryGetDependency_RequiresLock(DependencyRef);
		if (!Dependency)
		{
			continue;
		}

		ensure(Dependency->ReferencingTrackers.Set_ReturnOld(TrackerIndex, false) == true);
	}
}

FVoxelOnInvalidated FVoxelDependencyTracker::Invalidate()
{
	if (PrivateIsInvalidated.Set_ReturnOld(true) == true)
	{
		// Already invalidated
		return {};
	}

	UnregisterFromDependencies();

	return MoveTemp(OnInvalidated);
}