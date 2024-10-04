// Copyright Voxel Plugin SAS. All Rights Reserved.

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

	while (true)
	{
		LOG("####################################################");
		LOG("####################################################");
		LOG("####################################################");

		{
			int32 Value = 0;
			const TUniqueFunction<void()> EngineFunction = [&] { Value++; };
			const TVoxelUniqueFunction<void()> VoxelFunction = [&] { Value++; };

			RunBenchmark(
				"Calling TUniqueFunction",
				1000000,
				nullptr,
				nullptr,
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineFunction();
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelFunction();
					}
				});
		}

		{
			int32 Value = 0;
			TMap<int32, int32> EngineMap;
			TVoxelMap<int32, int32> VoxelMap;
			EngineMap.Reserve(1000000);
			VoxelMap.Reserve(1000000);

			for (int32 Index = 0; Index < 1000000; Index++)
			{
				EngineMap.Add(Index, Index);
				VoxelMap.Add_CheckNew(Index, Index);
			}

			RunBenchmark(
				"TMap::FindChecked",
				1000000,
				nullptr,
				nullptr,
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						Value += EngineMap.FindChecked(Run);
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						Value += VoxelMap.FindChecked(Run);
					}
				});
		}

		{
			constexpr int32 NumInnerRuns = 100000;

			TMap<int32, int32> EngineMap;
			TVoxelMap<int32, int32> VoxelMap;

			RunBenchmark(
				"TMap::Remove",
				NumInnerRuns,
				[&]
				{
					EngineMap.Empty();
					EngineMap.Reserve(NumInnerRuns);

					for (int32 Index = 0; Index < NumInnerRuns; Index++)
					{
						EngineMap.Add(Index, Index);
					}
				},
				[&]
				{
					VoxelMap.Empty();
					VoxelMap.Reserve(NumInnerRuns);

					for (int32 Index = 0; Index < NumInnerRuns; Index++)
					{
						VoxelMap.Add_CheckNew(Index, Index);
					}
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineMap.Remove(Stream.RandRange(0, NumRuns - 1));
					}
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelMap.Remove(Stream.RandRange(0, NumRuns - 1));
					}
				});
		}

		{
			TMap<int32, int32> EngineMap;
			TVoxelMap<int32, int32> VoxelMap;

			RunBenchmark(
				"TMap::Reserve(1M)",
				1,
				[&]
				{
					EngineMap.Empty();
				},
				[&]
				{
					VoxelMap.Empty();
				},
				[&](const int32)
				{
					EngineMap.Reserve(1000000);
				},
				[&](const int32)
				{
					VoxelMap.Reserve(1000000);
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TMap<int32, int32> EngineMap;
			TVoxelMap<int32, int32> VoxelMap;

			RunBenchmark(
				"TMap::FindOrAdd",
				NumInnerRuns,
				[&]
				{
					EngineMap.Empty();
					EngineMap.Reserve(NumInnerRuns);
				},
				[&]
				{
					VoxelMap.Empty();
					VoxelMap.Reserve(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineMap.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32));
					}
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelMap.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32));
					}
				});
		}

		{
			constexpr int32 NumInnerRuns = 100000;

			TMap<FIntVector, int32> EngineMap;
			TVoxelMap<FIntVector, int32> VoxelMap;

			RunBenchmark(
				"TMap::FindOrAdd<FIntVector>",
				NumInnerRuns,
				[&]
				{
					EngineMap.Empty();
					EngineMap.Reserve(NumInnerRuns);
				},
				[&]
				{
					VoxelMap.Empty();
					VoxelMap.Reserve(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineMap.FindOrAdd(FIntVector(Stream.RandRange(MIN_int32, MAX_int32)));
					}
				},
				[&](const int32 NumRuns)
				{
					FRandomStream Stream;
					Stream.Initialize(1337);

					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelMap.FindOrAdd(FIntVector(Stream.RandRange(MIN_int32, MAX_int32)));
					}
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TMap<int32, int32> EngineMap;
			TVoxelMap<int32, int32> VoxelMap;

			RunBenchmark(
				"TMap::Add_CheckNew",
				NumInnerRuns,
				[&]
				{
					EngineMap.Empty();
					EngineMap.Reserve(NumInnerRuns);
				},
				[&]
				{
					VoxelMap.Empty();
					VoxelMap.Reserve(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineMap.Add(NumRuns);
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelMap.Add_CheckNew(NumRuns);
					}
				});
		}

		{
			TSparseArray<int32> EngineArray;
			TVoxelSparseArray<int32> VoxelArray;

			RunBenchmark(
				"TSparseArray::Reserve(1M)",
				1,
				[&]
				{
					EngineArray.Empty();
				},
				[&]
				{
					VoxelArray.Empty();
				},
				[&](const int32)
				{
					EngineArray.Reserve(1000000);
				},
				[&](const int32)
				{
					VoxelArray.Reserve(1000000);
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TSparseArray<int32> EngineArray;
			TVoxelSparseArray<int32> VoxelArray;

			RunBenchmark(
				"TSparseArray::Add",
				NumInnerRuns,
				[&]
				{
					EngineArray.Empty();
					EngineArray.Reserve(NumInnerRuns);
				},
				[&]
				{
					VoxelArray.Empty();
					VoxelArray.Reserve(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineArray.Add(NumRuns);
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelArray.Add(NumRuns);
					}
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TBitArray EngineArray;
			FVoxelBitArray32 VoxelArray;

			RunBenchmark(
				"TBitArray::Add",
				NumInnerRuns,
				[&]
				{
					EngineArray.Empty();
					EngineArray.Reserve(NumInnerRuns);
				},
				[&]
				{
					VoxelArray.Empty();
					VoxelArray.Reserve(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineArray.Add(false);
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelArray.Add(false);
					}
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TBitArray EngineArray;
			FVoxelBitArray32 VoxelArray;
			EngineArray.Reserve(NumInnerRuns);
			VoxelArray.Reserve(NumInnerRuns);

			FRandomStream Stream;
			Stream.Initialize(1337);
			for (int32 Index = 0; Index < NumInnerRuns; Index++)
			{
				const bool bValue = Stream.GetFraction() < 0.5f;
				EngineArray.Add(bValue);
				VoxelArray.Add(bValue);
			}

			int32 Value = 0;

			RunBenchmark(
				"TBitArray::CountSetBits",
				1,
				nullptr,
				nullptr,
				[&](const int32)
				{
					Value += EngineArray.CountSetBits();
				},
				[&](const int32)
				{
					Value += VoxelArray.CountSetBits();
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TBitArray EngineArray;
			FVoxelBitArray32 VoxelArray;
			EngineArray.Reserve(NumInnerRuns);
			VoxelArray.Reserve(NumInnerRuns);

			FRandomStream Stream;
			Stream.Initialize(1337);

			int32 Type = 0;
			for (int32 Index = 0; Index < NumInnerRuns; Index++)
			{
				// Try to generate not fully random patterns
				if (Index % 100 == 0)
				{
					Type = Stream.RandRange(0, 2);
				}

				if (Type == 0)
				{
					EngineArray.Add(false);
					VoxelArray.Add(false);
				}
				else if (Type == 1)
				{
					EngineArray.Add(true);
					VoxelArray.Add(true);
				}
				else
				{
					// Random
					const bool bValue = Stream.GetFraction() < 0.5f;
					EngineArray.Add(bValue);
					VoxelArray.Add(bValue);
				}
			}

			int32 Value = 0;

			RunBenchmark(
				"TConstSetBitIterator",
				1,
				nullptr,
				nullptr,
				[&](const int32)
				{
					for (TConstSetBitIterator<> It(EngineArray); It; ++It)
					{
						Value += It.GetIndex();
					}
				},
				[&](const int32)
				{
					VoxelArray.ForAllSetBits([&](const int32 Index)
					{
						Value += Index;
					});
				});
		}

		{
			constexpr int32 NumInnerRuns = 1000000;

			TArray<int32> EngineArray;
			TVoxelArray<int32> VoxelArray;

			RunBenchmark(
				"TArray::RemoveAtSwap",
				NumInnerRuns,
				[&]
				{
					EngineArray.Empty();
					EngineArray.SetNumZeroed(NumInnerRuns);
				},
				[&]
				{
					VoxelArray.Empty();
					VoxelArray.SetNumZeroed(NumInnerRuns);
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						EngineArray.RemoveAtSwap(0, 1, UE_505_SWITCH(false, EAllowShrinking::No));
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						VoxelArray.RemoveAtSwap(0);
					}
				});
		}

		{
			uint64 Value = 0;

			RunBenchmark(
				"AActor::StaticClass",
				1000000,
				nullptr,
				nullptr,
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						Value += uint64(AActor::StaticClass());
					}
				},
				[&](const int32 NumRuns)
				{
					for (int32 Run = 0; Run < NumRuns; Run++)
					{
						Value += uint64(StaticClassFast<AActor>());
					}
				});
		}

		{
			TMap<uint16, uint16> EngineMap;
			TVoxelMap<uint16, uint16> VoxelMap;
			EngineMap.Reserve(1000000);
			VoxelMap.Reserve(1000000);

			for (int32 Index = 0; Index < 1000000; Index++)
			{
				EngineMap.Add(Index, Index);
				VoxelMap.Add_CheckNew(Index, Index);
			}

			LOG("TMap<uint16, uint16> with 1M elements: Engine: %s Voxel: %s",
				*FVoxelUtilities::BytesToString(EngineMap.GetAllocatedSize()),
				*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
		}

		{
			TMap<uint32, uint32> EngineMap;
			TVoxelMap<uint32, uint32> VoxelMap;
			EngineMap.Reserve(1000000);
			VoxelMap.Reserve(1000000);

			for (int32 Index = 0; Index < 1000000; Index++)
			{
				EngineMap.Add(Index, Index);
				VoxelMap.Add_CheckNew(Index, Index);
			}

			LOG("TMap<uint32, uint32> with 1M elements: Engine: %s Voxel: %s",
				*FVoxelUtilities::BytesToString(EngineMap.GetAllocatedSize()),
				*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
		}

		FPlatformProcess::Sleep(1);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename EngineLambdaType, typename VoxelLambdaType>
void FVoxelCoreBenchmark::RunBenchmark(
	const FString& Name,
	int32 NumInnerRuns,
	const TFunction<void()> InitializeEngine,
	const TFunction<void()> InitializeVoxel,
	EngineLambdaType EngineExecute,
	VoxelLambdaType VoxelExecute)
{
	constexpr int32 NumOuterRuns = 100;

	TVoxelWelfordVariance<double> EngineTime;
	for (int32 Run = 0; Run < NumOuterRuns; Run++)
	{
		if (InitializeEngine)
		{
			InitializeEngine();
		}

		const double StartTime = FPlatformTime::Seconds();
		FVoxelCoreBenchmark::RunInnerBenchmark(NumInnerRuns, EngineExecute);
		const double EndTime = FPlatformTime::Seconds();

		EngineTime.Add((EndTime - StartTime) / NumInnerRuns);
	}

	TVoxelWelfordVariance<double> VoxelTime;
	for (int32 Run = 0; Run < NumOuterRuns; Run++)
	{
		if (InitializeVoxel)
		{
			InitializeVoxel();
		}

		const double StartTime = FPlatformTime::Seconds();
		FVoxelCoreBenchmark::RunInnerBenchmark(NumInnerRuns, VoxelExecute);
		const double EndTime = FPlatformTime::Seconds();

		VoxelTime.Add((EndTime - StartTime) / NumInnerRuns);
	}

	LOG("%-30s %4.1fx %s    Engine: %-9s Voxel: %-9s std: %2.0f%% %2.0f%%",
		*Name,
		EngineTime.Average / VoxelTime.Average,
		VoxelTime.Average < EngineTime.Average ? TEXT("faster") : TEXT("slower"),
		*FVoxelUtilities::SecondsToString(EngineTime.Average, 1),
		*FVoxelUtilities::SecondsToString(VoxelTime.Average, 1),
		EngineTime.GetStd() / EngineTime.Average * 100,
		VoxelTime.GetStd() / VoxelTime.Average * 100);
}

template<typename LambdaType>
FORCENOINLINE void FVoxelCoreBenchmark::RunInnerBenchmark(
	const int32 NumRuns,
	LambdaType Lambda)
{
	Lambda(NumRuns);
}

#undef LOG