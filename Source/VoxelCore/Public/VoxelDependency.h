// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

DECLARE_UNIQUE_VOXEL_ID(FVoxelDependencyId);

class VOXELCORE_API FVoxelDependencyBase : public TSharedFromThis<FVoxelDependencyBase>
{
public:
	const FString Name;
	const FVoxelDependencyId DependencyId;

	VOXEL_COUNT_INSTANCES();

	UE_NONCOPYABLE(FVoxelDependencyBase);

protected:
	explicit FVoxelDependencyBase(const FString& Name);
};

class VOXELCORE_API FVoxelDependency : public FVoxelDependencyBase
{
public:
	static TSharedRef<FVoxelDependency> Create(const FString& Name);

	void Invalidate();

private:
	using FVoxelDependencyBase::FVoxelDependencyBase;
};

class VOXELCORE_API FVoxelDependency2D : public FVoxelDependencyBase
{
public:
	static TSharedRef<FVoxelDependency2D> Create(const FString& Name);

	void Invalidate(const FVoxelBox2D& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox2D> BoundsArray);

private:
	using FVoxelDependencyBase::FVoxelDependencyBase;
};

class VOXELCORE_API FVoxelDependency3D : public FVoxelDependencyBase
{
public:
	static TSharedRef<FVoxelDependency3D> Create(const FString& Name);

	void Invalidate(const FVoxelBox& Bounds);
	void Invalidate(TConstVoxelArrayView<FVoxelBox> BoundsArray);

private:
	using FVoxelDependencyBase::FVoxelDependencyBase;
};