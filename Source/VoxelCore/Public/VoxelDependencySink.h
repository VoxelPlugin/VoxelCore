// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelDependencySink
{
public:
	FVoxelDependencySink();
	~FVoxelDependencySink();

public:
	static bool TryAddAction(
		TVoxelUniqueFunction<void()>&& Lambda,
		void* UniqueOwner = nullptr);

	static void AddAction(
		TVoxelUniqueFunction<void()> Lambda,
		void* UniqueOwner = nullptr);
};