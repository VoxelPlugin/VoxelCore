// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependency.h"
#include "VoxelDependencyManager.h"
#include "VoxelAABBTree.h"
#include "VoxelAABBTree2D.h"

DEFINE_UNIQUE_VOXEL_ID(FVoxelDependencyId);
DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDependencyBase);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyBase::FVoxelDependencyBase(const FString& Name)
	: Name(Name)
	, DependencyId(FVoxelDependencyId::New())
{
}

bool FVoxelDependencyBase::ShouldSkipInvalidate()
{
	// Skipping invalidation when we have no trackers is critical,
	// as moving a stamp can lead to continuous invalidations, even though the new state is not computed yet

	if (!HasTrackers.Get())
	{
		return true;
	}

	HasTrackers.Set(false);
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependency::Invalidate()
{
	VOXEL_FUNCTION_COUNTER();

	if (ShouldSkipInvalidate())
	{
		return;
	}

	GVoxelDependencyManager->InvalidateTrackers(Name, [&](const FVoxelDependencyTracker& Tracker)
	{
		return Tracker.Dependencies_RequiresLock.Contains(DependencyId);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependency2D::Invalidate(const FVoxelBox2D& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (ShouldSkipInvalidate())
	{
		return;
	}

	GVoxelDependencyManager->InvalidateTrackers(Name, [&](const FVoxelDependencyTracker& Tracker)
	{
		const FVoxelBox2D* TrackerBounds = Tracker.Dependency2DToBounds_RequiresLock.Find(DependencyId);
		if (!TrackerBounds)
		{
			return false;
		}

		return Bounds.Intersects(*TrackerBounds);
	});
}

void FVoxelDependency2D::Invalidate(const TConstVoxelArrayView<FVoxelBox2D> BoundsArray)
{
	VOXEL_FUNCTION_COUNTER();

	if (ShouldSkipInvalidate())
	{
		return;
	}

	const TSharedRef<FVoxelAABBTree2D> Tree = FVoxelAABBTree2D::Create(BoundsArray);

	GVoxelDependencyManager->InvalidateTrackers(Name, [&](const FVoxelDependencyTracker& Tracker)
	{
		const FVoxelBox2D* TrackerBounds = Tracker.Dependency2DToBounds_RequiresLock.Find(DependencyId);
		if (!TrackerBounds)
		{
			return false;
		}

		return Tree->Intersects(*TrackerBounds);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependency3D::Invalidate(const FVoxelBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (ShouldSkipInvalidate())
	{
		return;
	}

	GVoxelDependencyManager->InvalidateTrackers(Name, [&](const FVoxelDependencyTracker& Tracker)
	{
		const FVoxelBox* TrackerBounds = Tracker.Dependency3DToBounds_RequiresLock.Find(DependencyId);
		if (!TrackerBounds)
		{
			return false;
		}

		return Bounds.Intersects(*TrackerBounds);
	});
}

void FVoxelDependency3D::Invalidate(const TConstVoxelArrayView<FVoxelBox> BoundsArray)
{
	VOXEL_FUNCTION_COUNTER();

	if (ShouldSkipInvalidate())
	{
		return;
	}

	const TSharedRef<FVoxelAABBTree> Tree = FVoxelAABBTree::Create(BoundsArray);

	GVoxelDependencyManager->InvalidateTrackers(Name, [&](const FVoxelDependencyTracker& Tracker)
	{
		const FVoxelBox* TrackerBounds = Tracker.Dependency3DToBounds_RequiresLock.Find(DependencyId);
		if (!TrackerBounds)
		{
			return false;
		}

		return Tree->Intersects(*TrackerBounds);
	});
}