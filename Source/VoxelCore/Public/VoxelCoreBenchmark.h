// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelCoreBenchmark
{
public:
	static void Run();

private:
	template<typename EngineLambdaType, typename VoxelLambdaType>
	static void RunBenchmark(
		const FString& Name,
		int32 NumInnerRuns,
		TFunction<void()> InitializeEngine,
		TFunction<void()> InitializeVoxel,
		EngineLambdaType EngineExecute,
		VoxelLambdaType VoxelExecute);

	template<typename LambdaType>
	static void RunInnerBenchmark(
		int32 NumRuns,
		LambdaType Lambda);
};