// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencySink.h"
#include "VoxelDependency.h"

struct FVoxelDependencySinkData
{
	FVoxelCriticalSection CriticalSection;

	struct FData
	{
		int32 NumDependencySinks = 0;
		TVoxelSet<void*> VisitedOwners;
		TVoxelChunkedArray<TVoxelUniqueFunction<void()>> QueuedActions;
	};
	FData Data_RequiresLock;
};
FVoxelDependencySinkData GVoxelDependencySinkData;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencySink::FVoxelDependencySink()
{
	VOXEL_FUNCTION_COUNTER()
	VOXEL_SCOPE_LOCK(GVoxelDependencySinkData.CriticalSection);

	GVoxelDependencySinkData.Data_RequiresLock.NumDependencySinks++;
}

FVoxelDependencySink::~FVoxelDependencySink()
{
	VOXEL_FUNCTION_COUNTER()

	FVoxelDependencyInvalidationScope InvalidationScope;

	TVoxelChunkedArray<TVoxelUniqueFunction<void()>> QueuedActions;
	{
		VOXEL_SCOPE_LOCK(GVoxelDependencySinkData.CriticalSection);

		FVoxelDependencySinkData::FData& Data = GVoxelDependencySinkData.Data_RequiresLock;

		Data.NumDependencySinks--;

		if (Data.NumDependencySinks > 0)
		{
			return;
		}
		ensure(Data.NumDependencySinks == 0);

		Data.VisitedOwners.Reset();
		QueuedActions = MoveTemp(Data.QueuedActions);
	}

	LOG_VOXEL(Verbose, "FVoxelDependencySink: Flushing %d actions", QueuedActions.Num());

	for (const TVoxelUniqueFunction<void()>& Action : QueuedActions)
	{
		Action();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelDependencySink::TryAddAction(
	TVoxelUniqueFunction<void()>&& Lambda,
	void* UniqueOwner)
{
	VOXEL_SCOPE_LOCK(GVoxelDependencySinkData.CriticalSection);

	FVoxelDependencySinkData::FData& Data = GVoxelDependencySinkData.Data_RequiresLock;
	ensure(Data.NumDependencySinks >= 0);

	if (Data.NumDependencySinks == 0)
	{
		return false;
	}

	if (UniqueOwner)
	{
		if (Data.VisitedOwners.Contains(UniqueOwner))
		{
			return true;
		}

		Data.VisitedOwners.Add(UniqueOwner);
	}

	Data.QueuedActions.Add(MoveTemp(Lambda));
	return true;
}

void FVoxelDependencySink::AddAction(
	TVoxelUniqueFunction<void()> Lambda,
	void* UniqueOwner)
{
	if (!TryAddAction(MoveTemp(Lambda), UniqueOwner))
	{
		Lambda();
	}
}