// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependency;
class FVoxelDependencyTracker;
class FVoxelTransformRefImpl;

class VOXELCORE_API FVoxelTransformRef
{
public:
	FORCEINLINE static FVoxelTransformRef Identity()
	{
		return {};
	}
	static FVoxelTransformRef Make(const AActor& Actor);
	static FVoxelTransformRef Make(const USceneComponent& Component);

	static void NotifyTransformChanged(const USceneComponent& Component);

public:
	FVoxelTransformRef() = default;

	bool IsIdentity() const;
	FMatrix Get(FVoxelDependencyTracker& DependencyTracker) const;
	FMatrix Get_NoDependency() const;
	FVoxelTransformRef Inverse() const;
	FVoxelTransformRef operator*(const FVoxelTransformRef& Other) const;

	DECLARE_DELEGATE_OneParam(FOnChanged, const FMatrix& NewTransform);
	void AddOnChanged(const FOnChanged& OnChanged, bool bFireNow = true) const;

public:
	FORCEINLINE bool operator==(const FVoxelTransformRef& Other) const
	{
		return
			bIsInverted == Other.bIsInverted &&
			Impl == Other.Impl;
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelTransformRef& Ref)
	{
		return GetTypeHash(Ref.Impl);
	}

private:
	FVoxelTransformRef(
		const bool bIsInverted,
		const TSharedPtr<FVoxelTransformRefImpl>& Impl)
		: bIsInverted(bIsInverted)
		, Impl(Impl)
	{
	}

	bool bIsInverted = false;
	TSharedPtr<FVoxelTransformRefImpl> Impl;
};