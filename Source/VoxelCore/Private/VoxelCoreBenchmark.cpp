// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelCoreBenchmark.h"
#include "VoxelWelfordVariance.h"
#include "Misc/OutputDeviceConsole.h"
#include "Framework/Application/SlateApplication.h"

#define LOG(Format, ...) GLogConsole->Serialize(*FString::Printf(TEXT(Format), ##__VA_ARGS__), ELogVerbosity::Display, "Voxel");

void FVoxelCoreBenchmark::Run()
{
	FSlateApplication::Get().GetActiveTopLevelWindow()->Minimize();
	GLogConsole->Show(true);

	LOG("####################################################");
	LOG("####################################################");
	LOG("####################################################");
	LOG("DO_CHECK=%d", DO_CHECK);
	LOG("VOXEL_DEBUG=%d", VOXEL_DEBUG);

	FString OSLabel, OSVersion;
	FPlatformMisc::GetOSVersions(OSLabel, OSVersion);
	LOG("OS: %s (%s)", *OSLabel, *OSVersion);
	LOG("CPU: %s", *FPlatformMisc::GetCPUBrand());
	LOG("GPU: %s", *FPlatformMisc::GetPrimaryGPUBrand());

	LOG("####################################################");
	LOG("####################################################");
	LOG("####################################################");

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

	LOG("%s: Engine: %.3fus ~ %.2f Voxel: %.3fus ~ %.2f ----- %2.1f%% faster",
		*Name,
		EngineTime.Average * 1000000,
		EngineTime.GetStd() * 1000000,
		VoxelTime.Average * 1000000,
		VoxelTime.GetStd() * 1000000,
		(EngineTime.Average - VoxelTime.Average) / EngineTime.Average * 100);
}

#undef LOG