// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelCoreBenchmark.h"
#include "VoxelWelfordVariance.h"

void FVoxelCoreBenchmark::Run()
{
	LOG_VOXEL(Display, "####################################################");
	LOG_VOXEL(Display, "####################################################");
	LOG_VOXEL(Display, "####################################################");
	LOG_VOXEL(Display, "VOXEL_DEBUG=%d", VOXEL_DEBUG);

	FString OSLabel, OSVersion;
	FPlatformMisc::GetOSVersions(OSLabel, OSVersion);
	LOG_VOXEL(Display, "OS: %s (%s)", *OSLabel, *OSVersion);
	LOG_VOXEL(Display, "CPU: %s", *FPlatformMisc::GetCPUBrand());
	LOG_VOXEL(Display, "GPU: %s", *FPlatformMisc::GetPrimaryGPUBrand());

	LOG_VOXEL(Display, "####################################################");
	LOG_VOXEL(Display, "####################################################");
	LOG_VOXEL(Display, "####################################################");

	RunBenchmark(
		"Calling TUniqueFunction 1000 times",
		[]
		{
			int32 Time = 0;
			const TUniqueFunction<void()> Function = [&] { Time++; };

			for (int32 Run = 0; Run < 1000; Run++)
			{
				Function();
			}

			check(Time == 1000);
		},
		[]
		{
			int32 Time = 0;
			const TVoxelUniqueFunction<void()> Function = [&] { Time++; };

			for (int32 Run = 0; Run < 1000; Run++)
			{
				Function();
			}

			check(Time == 1000);
		});

	while (true)
	{
		FPlatformProcess::Sleep(1);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelCoreBenchmark::RunBenchmark(
	const FString& Name,
	TVoxelUniqueFunction<void()> EngineExecute,
	TVoxelUniqueFunction<void()> VoxelExecute)
{
	TVoxelWelfordVariance<double> EngineTime;
	for (int32 Run = 0; Run < 10000; Run++)
	{
		const double StartTime = FPlatformTime::Seconds();
		EngineExecute();
		const double EndTime = FPlatformTime::Seconds();

		EngineTime.Add(EndTime - StartTime);
	}

	TVoxelWelfordVariance<double> VoxelTime;
	for (int32 Run = 0; Run < 10000; Run++)
	{
		const double StartTime = FPlatformTime::Seconds();
		VoxelExecute();
		const double EndTime = FPlatformTime::Seconds();

		VoxelTime.Add(EndTime - StartTime);
	}

	LOG_VOXEL(Display, "%s: Engine: %.3fus ~ %.3f Voxel: %.3fus ~ %.3f ----- %2.1f%% faster",
		*Name,
		EngineTime.Average * 1000000,
		EngineTime.GetStd() * 1000000,
		VoxelTime.Average * 1000000,
		VoxelTime.GetStd() * 1000000,
		(EngineTime.Average - VoxelTime.Average) / EngineTime.Average * 100);
}