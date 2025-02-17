// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

DECLARE_UNIQUE_VOXEL_ID(FVoxelDependencyId);

class VOXELCORE_API FVoxelDependencyBase
{
public:
	const FString Name;
	const FVoxelDependencyId DependencyId;

	explicit FVoxelDependencyBase(const FString& Name);
	UE_NONCOPYABLE(FVoxelDependencyBase);

	VOXEL_COUNT_INSTANCES();

protected:
	mutable TVoxelAtomic<bool> HasTrackers;

	bool ShouldSkipInvalidate();

	friend class FVoxelDependencyTracker;
};

class VOXELCORE_API FVoxelDependency : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	void Invalidate();
};

class VOXELCORE_API FVoxelDependency2D : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	void Invalidate(const FVoxelBox2D& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox2D> BoundsArray);
};

class VOXELCORE_API FVoxelDependency3D : public FVoxelDependencyBase
{
public:
	using FVoxelDependencyBase::FVoxelDependencyBase;

	void Invalidate(const FVoxelBox& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox> BoundsArray);
};