// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox2D.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelFastBox.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/Containers/VoxelArray.h"

#define VOXEL_INVALIDATION_TRACKING WITH_EDITOR

class FVoxelInvalidationCallstack;

using FVoxelOnInvalidated = TVoxelUniqueFunction<void(const FVoxelInvalidationCallstack& Callstack)>;

struct alignas(8) FVoxelDependencyRef
{
	int32 Index = 0;
	int32 SerialNumber = 0;

	FVoxelDependencyRef() = default;

	FORCEINLINE bool operator==(const FVoxelDependencyRef& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef<uint64>(Other);
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelDependencyRef& DependencyRef)
	{
		return DependencyRef.Index;
	}
};

DECLARE_VOXEL_MEMORY_STAT(VOXELCORE_API, STAT_VoxelDependencyTrackerMemory, "Voxel Dependency Tracker Memory");

class VOXELCORE_API FVoxelDependencyTracker
{
	struct FPrivate {};

public:
	explicit FVoxelDependencyTracker(FPrivate)
	{
	}
	~FVoxelDependencyTracker();

	UE_NONCOPYABLE(FVoxelDependencyTracker);

public:
	FORCEINLINE bool IsInvalidated() const
	{
		return PrivateIsInvalidated.Get();
	}

private:
	const int32 TrackerIndex = -1;
	FMinimalName Name;
	TVoxelAtomic<bool> PrivateIsInvalidated;
	TVoxelAtomic<bool> IsRegisteredToDependencies;

	VOXEL_COUNT_INSTANCES();

	FVoxelOnInvalidated OnInvalidated;

	TVoxelArray<FVoxelDependencyRef> AllDependencies;
	TVoxelArray<FVoxelBox2D> Bounds_2D;
	TVoxelArray<FVoxelFastBox> Bounds_3D;

	TConstVoxelArrayView<FVoxelDependencyRef> Dependencies;
	TConstVoxelArrayView<FVoxelDependencyRef> Dependencies_2D;
	TConstVoxelArrayView<FVoxelDependencyRef> Dependencies_3D;

	int64 GetAllocatedSize() const;

	void RegisterToDependencies();
	void UnregisterFromDependencies();
	[[nodiscard]] FVoxelOnInvalidated Invalidate();

	friend class FVoxelDependency;
	friend class FVoxelDependency2D;
	friend class FVoxelDependency3D;
	friend class FVoxelDependencyBase;
	friend class FVoxelDependencyManager;
	friend class FVoxelDependencyCollector;
};
checkStatic(sizeof(FVoxelDependencyTracker) == 128);