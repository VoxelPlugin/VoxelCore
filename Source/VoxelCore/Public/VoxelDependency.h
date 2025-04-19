// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelChunkedBitArrayTS.h"

class FVoxelAABBTree2D;
class FVoxelFastAABBTree;

class VOXELCORE_API FVoxelDependencyBase : public TSharedFromThis<FVoxelDependencyBase>
{
	struct FPrivate {};

public:
	const FString Name;

	VOXEL_COUNT_INSTANCES();

	FVoxelDependencyBase(
		FPrivate,
		const FString& Name)
		: Name(Name)
	{
	}
	UE_NONCOPYABLE(FVoxelDependencyBase);

	VOXEL_ALLOCATED_SIZE_TRACKER(STAT_VoxelDependencyTrackerMemory);

	int64 GetAllocatedSize() const;

protected:
	static TSharedRef<FVoxelDependencyBase> CreateImpl(const FString& Name);

	const FVoxelDependencyRef DependencyRef;
	FVoxelChunkedBitArrayTS ReferencingTrackers;

	template<typename LambdaType>
	void InvalidateTrackers(LambdaType ShouldInvalidate);

	friend FVoxelDependencyTracker;
	friend FVoxelDependencyCollector;
	friend class FVoxelDependencyManager;
};

class VOXELCORE_API FVoxelDependency : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	static TSharedRef<FVoxelDependency> Create(const FString& Name);

	void Invalidate();
};

class VOXELCORE_API FVoxelDependency2D : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	static TSharedRef<FVoxelDependency2D> Create(const FString& Name);

	void Invalidate(const FVoxelBox2D& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox2D> BoundsArray);
	void Invalidate(const TSharedRef<const FVoxelAABBTree2D>& Tree);
};

class VOXELCORE_API FVoxelDependency3D : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	static TSharedRef<FVoxelDependency3D> Create(const FString& Name);

	void Invalidate(const FVoxelBox& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox> BoundsArray);
	void Invalidate(const TSharedRef<const FVoxelFastAABBTree>& Tree);
};