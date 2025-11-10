// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelPropertyDiffing
{
public:
	static void Traverse(
		const FProperty& Property,
		const FString& BasePath,
		const void* OldMemory,
		const void* NewMemory,
		TVoxelArray<FString>& OutChanges);

private:
	static FString ToString(
		const FProperty& Property,
		const void* Memory);
};