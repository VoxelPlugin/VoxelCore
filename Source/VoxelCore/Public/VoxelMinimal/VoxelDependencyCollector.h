// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelMap.h"
#include "VoxelMinimal/VoxelCriticalSection.h"
#include "VoxelMinimal/VoxelDependencyTracker.h"

class FVoxelDependency;
class FVoxelDependency2D;
class FVoxelDependency3D;
class FVoxelDependencyBase;
class FVoxelInvalidationQueue;

class VOXELCORE_API FVoxelDependencyCollector
{
public:
	explicit FVoxelDependencyCollector(FName Name);

	static FVoxelDependencyCollector Null;

	UE_NONCOPYABLE(FVoxelDependencyCollector);

public:
	bool HasDependencies() const;

	void AddDependency(const FVoxelDependency& Dependency);

	void AddDependency(
		const FVoxelDependency2D& Dependency,
		const FVoxelBox2D& Bounds);

	void AddDependency(
		const FVoxelDependency3D& Dependency,
		const FVoxelBox& Bounds);

	void AddDependencies(const FVoxelDependencyCollector& Other);

public:
	TSharedRef<FVoxelDependencyTracker> Finalize(
		const FVoxelInvalidationQueue* InvalidationQueue,
		FVoxelOnInvalidated&& OnInvalidated);

private:
	const FName Name;
	const bool bIsNull;

	FVoxelCriticalSection CriticalSection;
	bool bFinalized = false;
	// Keep dependencies alive until we're finalized
	TVoxelArray<TSharedRef<const FVoxelDependencyBase>> SharedDependencies;

	TVoxelArray<FVoxelDependencyRef> Dependencies;
	TVoxelMap<FVoxelDependencyRef, FVoxelBox2D> Dependency2DToBounds;
	TVoxelMap<FVoxelDependencyRef, FVoxelBox> Dependency3DToBounds;

	explicit FVoxelDependencyCollector(ENoInit);

	TSharedRef<FVoxelDependencyTracker> CreateTracker(FVoxelOnInvalidated&& OnInvalidated) const;
};