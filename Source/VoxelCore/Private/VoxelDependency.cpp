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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependency> FVoxelDependency::Create(const FString& Name)
{
	return MakeShareable(new FVoxelDependency(Name));
}
void FVoxelDependency::Invalidate()
{
	VOXEL_FUNCTION_COUNTER();

	GVoxelDependencyManager->InvalidateTrackers(this, [&](const FVoxelDependencyTracker& Tracker)
	{
		return Tracker.Dependencies_RequiresLock.Contains(DependencyId);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependency2D> FVoxelDependency2D::Create(const FString& Name)
{
	return MakeShareable(new FVoxelDependency2D(Name));
}

void FVoxelDependency2D::Invalidate(const FVoxelBox2D& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensureVoxelSlow(Bounds.IsValidAndNotEmpty()))
	{
		return;
	}

	GVoxelDependencyManager->InvalidateTrackers(this, [=, this](const FVoxelDependencyTracker& Tracker)
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

	if (BoundsArray.Num() == 0)
	{
		return;
	}

	const TSharedRef<FVoxelAABBTree2D> Tree = FVoxelAABBTree2D::Create(BoundsArray);

	GVoxelDependencyManager->InvalidateTrackers(this, [=, this](const FVoxelDependencyTracker& Tracker)
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

TSharedRef<FVoxelDependency3D> FVoxelDependency3D::Create(const FString& Name)
{
	return MakeShareable(new FVoxelDependency3D(Name));
}

void FVoxelDependency3D::Invalidate(const FVoxelBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensureVoxelSlow(Bounds.IsValidAndNotEmpty()))
	{
		return;
	}

	GVoxelDependencyManager->InvalidateTrackers(this, [=, this](const FVoxelDependencyTracker& Tracker)
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

	if (BoundsArray.Num() == 0)
	{
		return;
	}

	const TSharedRef<FVoxelAABBTree> Tree = FVoxelAABBTree::Create(BoundsArray);

	GVoxelDependencyManager->InvalidateTrackers(this, [=, this](const FVoxelDependencyTracker& Tracker)
	{
		const FVoxelBox* TrackerBounds = Tracker.Dependency3DToBounds_RequiresLock.Find(DependencyId);
		if (!TrackerBounds)
		{
			return false;
		}

		return Tree->Intersects(*TrackerBounds);
	});
}