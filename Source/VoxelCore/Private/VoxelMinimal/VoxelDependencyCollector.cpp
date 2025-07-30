// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelDependency.h"
#include "VoxelDependencyManager.h"

FVoxelDependencyCollector FVoxelDependencyCollector::Null(NoInit);

FVoxelDependencyCollector::FVoxelDependencyCollector(const FName Name)
	: Name(Name)
	, bIsNull(false)
{
	VOXEL_FUNCTION_COUNTER();

	Dependencies.Reserve(16);
	Dependency2DToBounds.Reserve(16);
	Dependency3DToBounds.Reserve(16);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDependencyCollector::AddDependency(const FVoxelDependency& Dependency)
{
	if (bIsNull)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(CriticalSection);
	checkVoxelSlow(!bFinalized);

	if (Dependencies.Contains(Dependency.DependencyRef))
	{
		return;
	}

	SharedDependencies.Add(Dependency.AsShared());
	Dependencies.Add(Dependency.DependencyRef);
}

void FVoxelDependencyCollector::AddDependency(
	const FVoxelDependency2D& Dependency,
	const FVoxelBox2D& Bounds)
{
	if (bIsNull)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(CriticalSection);
	checkVoxelSlow(!bFinalized);

	if (FVoxelBox2D* ExistingBounds = Dependency2DToBounds.Find(Dependency.DependencyRef))
	{
		*ExistingBounds += Bounds;
		return;
	}

	SharedDependencies.Add(Dependency.AsShared());
	Dependency2DToBounds.Add_CheckNew(Dependency.DependencyRef, Bounds);
}

void FVoxelDependencyCollector::AddDependency(
	const FVoxelDependency3D& Dependency,
	const FVoxelBox& Bounds)
{
	if (bIsNull)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(CriticalSection);
	checkVoxelSlow(!bFinalized);

	if (FVoxelBox* ExistingBounds = Dependency3DToBounds.Find(Dependency.DependencyRef))
	{
		*ExistingBounds += Bounds;
		return;
	}

	SharedDependencies.Add(Dependency.AsShared());
	Dependency3DToBounds.Add_CheckNew(Dependency.DependencyRef, Bounds);
}

void FVoxelDependencyCollector::AddDependencies(const FVoxelDependencyCollector& Other)
{
	if (bIsNull ||
		!Other.HasDependencies())
	{
		return;
	}

	VOXEL_FUNCTION_COUNTER_COND(Other.SharedDependencies.Num() > 16);
	VOXEL_SCOPE_LOCK(CriticalSection);
	VOXEL_SCOPE_LOCK(Other.CriticalSection);

	for (const TSharedRef<const FVoxelDependencyBase>& Dependency : Other.SharedDependencies)
	{
		SharedDependencies.AddUnique(Dependency);
	}

	for (const FVoxelDependencyRef& Dependency : Other.Dependencies)
	{
		Dependencies.AddUnique(Dependency);
	}

	for (const auto& It : Other.Dependency2DToBounds)
	{
		if (FVoxelBox2D* Bounds = Dependency2DToBounds.Find(It.Key))
		{
			*Bounds += It.Value;
		}
		else
		{
			Dependency2DToBounds.Add_CheckNew(It.Key, It.Value);
		}
	}

	for (const auto& It : Other.Dependency3DToBounds)
	{
		if (FVoxelBox* Bounds = Dependency3DToBounds.Find(It.Key))
		{
			*Bounds += It.Value;
		}
		else
		{
			Dependency3DToBounds.Add_CheckNew(It.Key, It.Value);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDependencyTracker> FVoxelDependencyCollector::Finalize(
	const FVoxelInvalidationQueue* InvalidationQueue,
	FVoxelOnInvalidated&& OnInvalidated)
{
	VOXEL_FUNCTION_COUNTER();
	check(!bIsNull);

	check(!bFinalized);
	bFinalized = true;

	const TSharedRef<FVoxelDependencyTracker> Result = CreateTracker(MoveTemp(OnInvalidated));

	if (InvalidationQueue)
	{
		if (const TSharedPtr<const FVoxelInvalidationCallstack> Callstack = InvalidationQueue->FindInvalidation(*Result))
		{
			if (const FVoxelOnInvalidated LocalOnInvalidated = Result->Invalidate())
			{
				LocalOnInvalidated(*Callstack);
			}
		}
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyCollector::FVoxelDependencyCollector(ENoInit)
	: Name(NAME_None)
	, bIsNull(true)
{
}

TSharedRef<FVoxelDependencyTracker> FVoxelDependencyCollector::CreateTracker(FVoxelOnInvalidated&& OnInvalidated) const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	FVoxelDependencyTracker& Tracker = GVoxelDependencyManager->AllocateDependencyTracker();

	Tracker.Name = FMinimalName(Name);
	Tracker.OnInvalidated = MoveTemp(OnInvalidated);

	Tracker.AllDependencies.Reserve(
		Dependencies.Num() +
		Dependency2DToBounds.Num() +
		Dependency3DToBounds.Num());

	Tracker.Bounds_2D.Reserve(Dependency2DToBounds.Num());
	Tracker.Bounds_3D.Reserve(Dependency3DToBounds.Num());

	Tracker.AllDependencies.Append(Dependencies);

	const int32 Dependencies2DStart = Tracker.AllDependencies.Num();

	for (const auto& It : Dependency2DToBounds)
	{
		Tracker.AllDependencies.Add_EnsureNoGrow(It.Key);
		Tracker.Bounds_2D.Add_EnsureNoGrow(It.Value);
	}

	const int32 Dependencies3DStart = Tracker.AllDependencies.Num();

	for (const auto& It : Dependency3DToBounds)
	{
		Tracker.AllDependencies.Add_EnsureNoGrow(It.Key);
		Tracker.Bounds_3D.Add_EnsureNoGrow(FVoxelFastBox(It.Value));
	}

	Tracker.Dependencies = Tracker.AllDependencies.View().Slice(0, Dependencies.Num());
	Tracker.Dependencies_2D = Tracker.AllDependencies.View().Slice(Dependencies2DStart, Dependency2DToBounds.Num());
	Tracker.Dependencies_3D = Tracker.AllDependencies.View().Slice(Dependencies3DStart, Dependency3DToBounds.Num());

	Tracker.RegisterToDependencies();

	INC_VOXEL_MEMORY_STAT_BY(STAT_VoxelDependencyTrackerMemory, Tracker.GetAllocatedSize());

	return MakeShareable_CustomDestructor(&Tracker, [&Tracker]
	{
		VOXEL_SCOPE_COUNTER("FVoxelDependencyTracker::~FVoxelDependencyTracker");
		DEC_VOXEL_MEMORY_STAT_BY(STAT_VoxelDependencyTrackerMemory, Tracker.GetAllocatedSize());

		Tracker.UnregisterFromDependencies();
		GVoxelDependencyManager->FreeDependencyTracker(Tracker);
	});
}