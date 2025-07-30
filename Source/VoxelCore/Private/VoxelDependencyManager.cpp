// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDependencyManager.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.DumpDependencies",
	"Dump all depedencies, more effective if voxel.TrackAllPromisesCallstacks is true")
{
	GVoxelDependencyManager->Dump();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyManager* GVoxelDependencyManager = new FVoxelDependencyManager();

void FVoxelDependencyManager::Dump() const
{
	VOXEL_FUNCTION_COUNTER();

	VOXEL_SCOPE_READ_LOCK(DependencyTrackers_CriticalSection);
	VOXEL_SCOPE_READ_LOCK(Dependencies_CriticalSection);

	LOG_VOXEL(Log, "Num dependencies: %d", Dependencies_RequiresLock.Num());
	LOG_VOXEL(Log, "Num dependency tackers: %d", DependencyTrackers_RequiresLock.Num());

	{
		TVoxelMap<FName, int32> NameToCount;
		NameToCount.Reserve(1024);

		for (const FVoxelDependencyBase& Dependency : Dependencies_RequiresLock)
		{
			NameToCount.FindOrAdd(FName(Dependency.Name))++;
		}

		NameToCount.ValueSort([](const int32 A, const int32 B)
		{
			return A > B;
		});

		LOG_VOXEL(Log, "Dependencies:");

		for (const auto& It : NameToCount)
		{
			LOG_VOXEL(Log, "\t%s x%d", *It.Key.ToString(), It.Value);
		}
	}

	{
		TVoxelMap<FName, int32> NameToCount;
		NameToCount.Reserve(1024);

		for (const FVoxelDependencyTracker* DependencyTracker : DependencyTrackers_RequiresLock)
		{
			NameToCount.FindOrAdd(FName(DependencyTracker->Name))++;
		}

		NameToCount.ValueSort([](const int32 A, const int32 B)
		{
			return A > B;
		});

		LOG_VOXEL(Log, "Dependency trackers:");

		for (const auto& It : NameToCount)
		{
			LOG_VOXEL(Log, "\t%s x%d", *It.Key.ToString(), It.Value);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyBase& FVoxelDependencyManager::AllocateDependency(const FString& Name)
{
	VOXEL_SCOPE_READ_LOCK(DependencyTrackers_CriticalSection);
	VOXEL_SCOPE_WRITE_LOCK(Dependencies_CriticalSection);

	static FVoxelCounter32 StaticSerialNumber = 1000;

	const int32 Index = Dependencies_RequiresLock.Emplace(FVoxelDependencyBase::FPrivate(), Name);
	UpdateStats();

	FVoxelDependencyBase& Dependency = Dependencies_RequiresLock[Index];
	ConstCast(Dependency.DependencyRef).Index = Index;
	ConstCast(Dependency.DependencyRef).SerialNumber = StaticSerialNumber.Increment_ReturnNew();

	Dependency.ReferencingTrackers.SetNumChunks(GVoxelDependencyManager->GetReferencersNumChunks());
	Dependency.UpdateStats();

	return Dependency;
}

void FVoxelDependencyManager::FreeDependency(const FVoxelDependencyBase& Dependency)
{
	VOXEL_SCOPE_WRITE_LOCK(Dependencies_CriticalSection);
	checkVoxelSlow(&Dependencies_RequiresLock[Dependency.DependencyRef.Index] == &Dependency);
	Dependencies_RequiresLock.RemoveAt(Dependency.DependencyRef.Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDependencyTracker& FVoxelDependencyManager::AllocateDependencyTracker()
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelDependencyTracker* Tracker = new FVoxelDependencyTracker(FVoxelDependencyTracker::FPrivate());

	{
		VOXEL_SCOPE_WRITE_LOCK(DependencyTrackers_CriticalSection);

		const int32 LastNumChunks = GetReferencersNumChunks();
		ConstCast(Tracker->TrackerIndex) = DependencyTrackers_RequiresLock.Emplace(Tracker);
		const int32 NewNumChunks = GetReferencersNumChunks();

		if (NewNumChunks != LastNumChunks)
		{
			VOXEL_SCOPE_COUNTER("SetNumChunks");
			VOXEL_SCOPE_READ_LOCK(Dependencies_CriticalSection);

			for (FVoxelDependencyBase& Dependency : Dependencies_RequiresLock)
			{
				Dependency.ReferencingTrackers.SetNumChunks(NewNumChunks);
				Dependency.UpdateStats();
			}

			UpdateStats();
		}
	}

	return *Tracker;
}

void FVoxelDependencyManager::FreeDependencyTracker(const FVoxelDependencyTracker& Tracker)
{
	VOXEL_FUNCTION_COUNTER();

	{
		VOXEL_SCOPE_WRITE_LOCK(DependencyTrackers_CriticalSection);
		checkVoxelSlow(DependencyTrackers_RequiresLock[Tracker.TrackerIndex] == &Tracker);
		DependencyTrackers_RequiresLock.RemoveAt(Tracker.TrackerIndex);
	}

	delete &Tracker;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int32 FVoxelDependencyManager::AddInvalidationQueue(FVoxelInvalidationQueue* InvalidationQueue)
{
	VOXEL_SCOPE_WRITE_LOCK(InvalidationQueues_CriticalSection);
	return InvalidationQueues_RequiresLock.Add(InvalidationQueue);
}

void FVoxelDependencyManager::RemoveInvalidationQueue(const int32 Index)
{
	VOXEL_SCOPE_WRITE_LOCK(InvalidationQueues_CriticalSection);
	InvalidationQueues_RequiresLock.RemoveAt(Index);
}