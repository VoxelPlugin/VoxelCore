// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelCoreBenchmark
{
public:
	static void Run();

private:
static void RunBenchmark(
	const FString& Name,
	TVoxelUniqueFunction<void()> EngineExecute,
	TVoxelUniqueFunction<void()> VoxelExecute);
};