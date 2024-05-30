// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelDependency.h"

class VOXELCORE_API FVoxelDependencyTracker : public TSharedFromThis<FVoxelDependencyTracker>
{
public:
	const FName Name;

	static TSharedRef<FVoxelDependencyTracker> Create(const FName Name)
	{
		return MakeVoxelShareable(new (GVoxelMemory) FVoxelDependencyTracker(Name));
	}
	~FVoxelDependencyTracker();
	UE_NONCOPYABLE(FVoxelDependencyTracker);

	VOXEL_COUNT_INSTANCES();

	FORCEINLINE bool IsInvalidated() const
	{
		return bIsInvalidated.Get();
	}
	void AddDependency(
		const TSharedRef<FVoxelDependency>& Dependency,
		const TOptional<FVoxelBox>& Bounds = {},
		const TOptional<uint64>& Tag = {});

	// Returns false if already invalidated
	bool TrySetOnInvalidated(TVoxelUniqueFunction<void()> NewOnInvalidated);
	// Will run inline if invalidated
	void SetOnInvalidated(TVoxelUniqueFunction<void()> NewOnInvalidated);

	template<typename T>
	void AddObjectToKeepAlive(const TSharedPtr<T>& ObjectToKeepAlive)
	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		ObjectsToKeepAlive.Add(MakeSharedVoidPtr(ObjectToKeepAlive));
	}

private:
	struct FDependencyRef
	{
		TWeakPtr<FVoxelDependency> WeakDependency;
		int32 Index = -1;
	};

	FVoxelCriticalSection CriticalSection;
	TVoxelAtomic<bool> bIsInvalidated;
	TVoxelUniqueFunction<void()> OnInvalidated;
	TVoxelChunkedArray<FDependencyRef> DependencyRefs;
	TVoxelArray<FSharedVoidPtr> ObjectsToKeepAlive;
	TVoxelMap<TWeakPtr<FVoxelDependency>, TVoxelInlineArray<FVoxelDependency::FTrackerRef, 2>> DependencyToTrackerRefs;

	explicit FVoxelDependencyTracker(const FName& Name);

	void Unregister_RequiresLock();

	friend FVoxelDependency;
	friend FVoxelDependencyInvalidationScope;
};