// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelWelfordVariance.h"
#include "Misc/OutputDeviceConsole.h"
#include "Framework/Application/SlateApplication.h"

// Set this to 1 and package in shipping to run the benchmark
#if 0

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define LOG(Format, ...) GLogConsole->Serialize(*FString::Printf(TEXT(Format), ##__VA_ARGS__), ELogVerbosity::Display, "Voxel");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace VoxelCoreBenchmark
{

template<typename LambdaType>
FORCENOINLINE void RunInnerBenchmark(LambdaType Lambda)
{
	Lambda();
}

template<int32 NumRuns, typename ExecuteAType, typename ExecuteBType>
requires
(
	LambdaHasSignature_V<ExecuteAType, void()> &&
	LambdaHasSignature_V<ExecuteBType, void()>
)
FORCENOINLINE void RunBenchmark(
	const FString& NameA,
	const TFunction<void()> InitializeA,
	ExecuteAType ExecuteA,
	const FString& NameB,
	const TFunction<void()> InitializeB,
	ExecuteBType ExecuteB, \
	const FString& Comment = {})
{
	const auto TrashCache = [&]
	{
#if !VOXEL_DEBUG // Too slow
		TVoxelArray<uint8> CacheTrash;
		FVoxelUtilities::SetNumFast(CacheTrash, 100 * 1000 * 1000);

		for (int32 Index = 0; Index < CacheTrash.Num(); Index++)
		{
			CacheTrash[Index] = CacheTrash[CacheTrash.Num() - 1 - Index];
		}
#endif
	};

	TVoxelWelfordVariance<double> TimeA;
	TVoxelWelfordVariance<double> TimeB;
	for (int32 Run = 0; Run < 100; Run++)
	{
		if (InitializeA)
		{
			InitializeA();
		}

		if (InitializeB)
		{
			InitializeB();
		}

		TrashCache();

		{
			const double StartTime = FPlatformTime::Seconds();
			RunInnerBenchmark(ExecuteA);
			const double EndTime = FPlatformTime::Seconds();

			TimeA.Add((EndTime - StartTime) / NumRuns);
		}

		TrashCache();

		{
			const double StartTime = FPlatformTime::Seconds();
			RunInnerBenchmark(ExecuteB);
			const double EndTime = FPlatformTime::Seconds();

			TimeB.Add((EndTime - StartTime) / NumRuns);
		}
	}

	const auto Print = [](double Seconds, double Std)
	{
		const TCHAR* Unit = TEXT("s");

		if (Seconds > 60)
		{
			Unit = TEXT("m");
			Seconds /= 60;
			Std /= 60;

			if (Seconds > 60)
			{
				Unit = TEXT("h");
				Seconds /= 60;
				Std /= 60;
			}
		}
		else if (Seconds < 1)
		{
			Unit = TEXT("ms");
			Seconds *= 1000;
			Std *= 1000;

			if (Seconds < 1)
			{
				Unit = TEXT("us");
				Seconds *= 1000;
				Std *= 1000;

				if (Seconds < 1)
				{
					Unit = TEXT("ns");
					Seconds *= 1000;
					Std *= 1000;
				}
			}
		}

		int32 NumDigits = 1;
		if (Seconds > 0)
		{
			while (Seconds * FVoxelUtilities::IntPow(10, NumDigits) < 1)
			{
				NumDigits++;
			}
		}

		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = NumDigits + 1;
		Options.MinimumFractionalDigits = NumDigits;

		FNumberFormattingOptions StdOptions;
		StdOptions.MaximumFractionalDigits = 1;
		StdOptions.MinimumFractionalDigits = 0;

		return FString::Printf(TEXT("%-9s +- %s"),
			*(FText::AsNumber(Seconds, &Options).ToString() + Unit),
			*FText::AsNumber(Std, &StdOptions).ToString());
	};

	LOG("%-50s took %-20s and %-50s took %-20s ====> %4.1fx %s",
		*NameA,
		*Print(TimeA.Average, TimeA.GetStd()),
		*NameB,
		*Print(TimeB.Average, TimeB.GetStd()),
		TimeA.Average / TimeB.Average,
		TimeB.Average < TimeA.Average ? TEXT("faster") : TEXT("slower"));

	if (!Comment.IsEmpty())
	{
		LOG("\tNote: %s", *Comment);
	}
}

template<int32 NumRuns, typename ExecuteAType, typename ExecuteBType>
requires
(
	LambdaHasSignature_V<ExecuteAType, void()> &&
	LambdaHasSignature_V<ExecuteBType, void()>
)
FORCENOINLINE void RunBenchmark(
	const FString& NameA,
	ExecuteAType ExecuteA,
	const FString& NameB,
	ExecuteBType ExecuteB, \
	const FString& Comment = {})
{
	RunBenchmark<NumRuns>(
		NameA,
		nullptr,
		ExecuteA,
		NameB,
		nullptr,
		ExecuteB,
		Comment);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelMap<int32, TFunction<void()>> GRunBenchmarks;

VOXEL_RUN_ON_STARTUP_GAME()
{
	if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow())
	{
		Window->Minimize();
	}

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

		GRunBenchmarks.KeySort();

		for (const auto& It : GRunBenchmarks)
		{
			It.Value();
		}

		FPlatformProcess::Sleep(1);
	}
}

#define CUSTOM_BENCHMARK \
	struct VOXEL_APPEND_LINE(FBenchmark) \
	{ \
		static void VOXEL_APPEND_LINE(Benchmark)(); \
	}; \
	VOXEL_RUN_ON_STARTUP(Game, 999) \
	{ \
		GRunBenchmarks.Add_CheckNew(__LINE__, VOXEL_APPEND_LINE(FBenchmark)::VOXEL_APPEND_LINE(Benchmark)); \
	} \
	void VOXEL_APPEND_LINE(FBenchmark)::VOXEL_APPEND_LINE(Benchmark)()

#define BENCHMARK \
	struct VOXEL_APPEND_LINE(FBenchmark) \
	{ \
		static constexpr int32 Num = VOXEL_DEBUG ? 100000 : 1000000; \
		\
		template<typename ExecuteAType, typename ExecuteBType> \
		FORCENOINLINE static void Run( \
			const FString& NameA, \
			const TFunction<void()> InitializeA, \
			ExecuteAType ExecuteA, \
			const FString& NameB, \
			const TFunction<void()> InitializeB, \
			ExecuteBType ExecuteB, \
			const FString& Comment = {}) \
		{ \
			RunBenchmark<Num>(NameA, InitializeA, ExecuteA, NameB, InitializeB, ExecuteB, Comment); \
		} \
		static void VOXEL_APPEND_LINE(Benchmark)(); \
	}; \
	VOXEL_RUN_ON_STARTUP(Game, 999) \
	{ \
		GRunBenchmarks.Add_CheckNew(__LINE__, VOXEL_APPEND_LINE(FBenchmark)::VOXEL_APPEND_LINE(Benchmark)); \
	} \
	void VOXEL_APPEND_LINE(FBenchmark)::VOXEL_APPEND_LINE(Benchmark)()

#define RUN_BENCHMARK(NameA, ExecuteA, NameB, ExecuteB) \
	RunBenchmark<Num>( \
		NameA, \
		[&] \
		{ \
			for (int32 Run = 0; Run < Num; Run++) \
			{ \
				ExecuteA; \
			} \
		}, \
		NameB, \
		[&] \
		{ \
			for (int32 Run = 0; Run < Num; Run++) \
			{ \
				ExecuteB; \
			} \
		});

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	int32 Value = 0;
	const TUniqueFunction<void()> Function = [&] { Value++; };
	const TVoxelUniqueFunction<void()> VoxelFunction = [&] { Value++; };

	RUN_BENCHMARK(
		"Calling TUniqueFunction",
		Function(),
		"Calling TVoxelUniqueFunction",
		VoxelFunction());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	TMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;
	{
		Map.Reserve(Num);
		VoxelMap.Reserve(Num);

		for (int32 Index = 0; Index < Num; Index++)
		{
			Map.Add(Index, Index);
			VoxelMap.Add_CheckNew(Index, Index);
		}
	}

	int32 Value = 0;

	RUN_BENCHMARK(
		"TMap::FindChecked",
		Value += Map.FindChecked(Run),
		"TVoxelMap::FindChecked",
		Value += VoxelMap.FindChecked(Run));
}

BENCHMARK
{
	TMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	Run(
		"TMap::Remove",
		[&]
		{
			Map.Empty();
			Map.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Map.Add(Index, Index);
			}
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				Map.Remove(Stream.RandRange(0, Num - 1));
			}
		},
		"TVoxelMap::Remove",
		[&]
		{
			VoxelMap.Empty();
			VoxelMap.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				VoxelMap.Add_CheckNew(Index, Index);
			}
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelMap.Remove(Stream.RandRange(0, Num - 1));
			}
		},
		"TVoxelMap::Remove does a RemoveAtSwap and doesn't keep order. TMap::Remove keeps order as long as no new element is added afterwards");
}

BENCHMARK
{
	TMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	Run(
		"TMap::FindOrAdd",
		[&]
		{
			Map.Empty();
			Map.Reserve(Num);
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				Map.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32));
			}
		},
		"TVoxelMap::FindOrAdd",
		[&]
		{
			VoxelMap.Empty();
			VoxelMap.Reserve(Num);
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelMap.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32));
			}
		});
}

BENCHMARK
{
	TMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	Run(
		"TMap::Add",
		[&]
		{
			Map.Empty();
			Map.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Map.Add(Run);
			}
		},
		"TVoxelMap::Add_CheckNew",
		[&]
		{
			VoxelMap.Empty();
			VoxelMap.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelMap.Add_CheckNew(Run);
			}
		},
		"TVoxelMap::Add_CheckNew can only be used when the element is not already in the map");
}

CUSTOM_BENCHMARK
{
	TMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	RunBenchmark<1>(
		"TMap::Reserve(1M)",
		[&]
		{
			Map.Empty();
		},
		[&]
		{
			Map.Reserve(1000000);
		},
		"TVoxelMap::Reserve(1M)",
		[&]
		{
			VoxelMap.Empty();
		},
		[&]
		{
			VoxelMap.Reserve(1000000);
		},
		"TMap::Reserve builds a linked list on reserve, TVoxelMap::Reserve is a memset");
}

CUSTOM_BENCHMARK
{
	{
		TMap<uint16, uint16> Map;
		TVoxelMap<uint16, uint16> VoxelMap;
		Map.Reserve(1000000);
		VoxelMap.Reserve(1000000);

		for (int32 Index = 0; Index < 1000000; Index++)
		{
			Map.Add(Index, Index);
			VoxelMap.FindOrAdd(Index) = Index;
		}

		LOG("TMap<uint16, uint16> with 1M elements: %s TVoxelMap<uint16, uint16> with 1M elements: %s",
			*FVoxelUtilities::BytesToString(Map.GetAllocatedSize()),
			*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
	}
	{
		TMap<uint32, uint32> Map;
		TVoxelMap<uint32, uint32> VoxelMap;
		Map.Reserve(1000000);
		VoxelMap.Reserve(1000000);

		for (int32 Index = 0; Index < 1000000; Index++)
		{
			Map.Add(Index, Index);
			VoxelMap.Add_CheckNew(Index, Index);
		}

		LOG("TMap<uint32, uint32> with 1M elements: %s TVoxelMap<uint32, uint32> with 1M elements: %s",
			*FVoxelUtilities::BytesToString(Map.GetAllocatedSize()),
			*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	Experimental::TRobinHoodHashMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;
	{
		Map.Reserve(Num);
		VoxelMap.Reserve(Num);

		for (int32 Index = 0; Index < Num; Index++)
		{
			Map.FindOrAdd(Index, Index);
			VoxelMap.Add_CheckNew(Index, Index);
		}
	}

	int32 Value = 0;

	RUN_BENCHMARK(
		"TRobinHoodHashMap::Find",
		Value += *Map.Find(Run),
		"TVoxelMap::FindChecked",
		Value += VoxelMap.FindChecked(Run));
}

BENCHMARK
{
	Experimental::TRobinHoodHashMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	Run(
		"TRobinHoodHashMap::Remove + Add",
		[&]
		{
			Map.Empty();
			Map.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Map.FindOrAdd(Index, Index);
			}
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				const int32 Value = Stream.RandRange(0, Num - 1);
				Map.Remove(Value);
				Map.FindOrAdd(Value, Value);
			}
		},
		"TVoxelMap::Remove + Add",
		[&]
		{
			VoxelMap.Empty();
			VoxelMap.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				VoxelMap.Add_CheckNew(Index, Index);
			}
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				const int32 Value = Stream.RandRange(0, Num - 1);
				VoxelMap.Remove(Value);
				VoxelMap.FindOrAdd(Value) = Value;
			}
		},
		"TRobinHoodHashMap::Remove can shrink the table. We add back the element we removed to avoid that.");
}

BENCHMARK
{
	Experimental::TRobinHoodHashMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	Run(
		"TRobinHoodHashMap::FindOrAdd",
		[&]
		{
			Map.Empty();
			Map.Reserve(Num);
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				Map.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32), Run);
			}
		},
		"TVoxelMap::FindOrAdd",
		[&]
		{
			VoxelMap.Empty();
			VoxelMap.Reserve(Num);
		},
		[&]
		{
			FRandomStream Stream;
			Stream.Initialize(1337);

			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelMap.FindOrAdd(Stream.RandRange(MIN_int32, MAX_int32)) = Run;
			}
		});
}

CUSTOM_BENCHMARK
{
	Experimental::TRobinHoodHashMap<int32, int32> Map;
	TVoxelMap<int32, int32> VoxelMap;

	RunBenchmark<1>(
		"TRobinHoodHashMap::Reserve(1M)",
		[&]
		{
			Map.Empty();
		},
		[&]
		{
			Map.Reserve(1000000);
		},
		"TVoxelMap::Reserve(1M)",
		[&]
		{
			VoxelMap.Empty();
		},
		[&]
		{
			VoxelMap.Reserve(1000000);
		});
}

CUSTOM_BENCHMARK
{
	{
		Experimental::TRobinHoodHashMap<uint16, uint16> Map;
		TVoxelMap<uint16, uint16> VoxelMap;
		Map.Reserve(1000000);
		VoxelMap.Reserve(1000000);

		for (int32 Index = 0; Index < 1000000; Index++)
		{
			Map.FindOrAdd(Index, Index);
			VoxelMap.FindOrAdd(Index) = Index;
		}

		LOG("TRobinHoodHashMap<uint16, uint16> with 1M elements: %s TVoxelMap<uint16, uint16> with 1M elements: %s",
			*FVoxelUtilities::BytesToString(Map.GetAllocatedSize()),
			*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
	}
	{
		Experimental::TRobinHoodHashMap<uint32, uint32> Map;
		TVoxelMap<uint32, uint32> VoxelMap;
		Map.Reserve(1000000);
		VoxelMap.Reserve(1000000);

		for (int32 Index = 0; Index < 1000000; Index++)
		{
			Map.FindOrAdd(Index, Index);
			VoxelMap.Add_CheckNew(Index, Index);
		}

		LOG("TRobinHoodHashMap<uint32, uint32> with 1M elements: %s TVoxelMap<uint32, uint32> with 1M elements: %s",
			*FVoxelUtilities::BytesToString(Map.GetAllocatedSize()),
			*FVoxelUtilities::BytesToString(VoxelMap.GetAllocatedSize()));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	TArray<int32> Array;
	TVoxelArray<int32> VoxelArray;

	Run(
		"TArray::RemoveAtSwap",
		[&]
		{
			Array.Empty();
			Array.SetNumZeroed(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Array.RemoveAtSwap(0, 1, UE_505_SWITCH(false, EAllowShrinking::No));
			}
		},
		"TVoxelArray::RemoveAtSwap",
		[&]
		{
			VoxelArray.Empty();
			VoxelArray.SetNumZeroed(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelArray.RemoveAtSwap(0);
			}
		},
		"TArray::RemoveAtSwap of one element does a memcpy with a non-compile-time size");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	TBitArray Array;
	FVoxelBitArray VoxelArray;

	Run(
		"TBitArray::Add",
		[&]
		{
			Array.Empty();
			Array.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Array.Add(false);
			}
		},
		"FVoxelBitArray::Add",
		[&]
		{
			VoxelArray.Empty();
			VoxelArray.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelArray.Add(false);
			}
		});
}

CUSTOM_BENCHMARK
{
	TBitArray Array;
	FVoxelBitArray VoxelArray;
	Array.Reserve(1000000);
	VoxelArray.Reserve(1000000);

	FRandomStream Stream;
	Stream.Initialize(1337);
	for (int32 Index = 0; Index < 1000000; Index++)
	{
		const bool bValue = Stream.GetFraction() < 0.5f;
		Array.Add(bValue);
		VoxelArray.Add(bValue);
	}

	int32 Value = 0;

	RunBenchmark<1>(
		"TBitArray::CountSetBits(1M)",
		[&]
		{
			Value += Array.CountSetBits();
		},
		"FVoxelBitArray::CountSetBits(1M)",
		[&]
		{
			Value += VoxelArray.CountSetBits();
		},
		"FVoxelBitArray::CountSetBits makes use of the popcount intrinsics");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BENCHMARK
{
	TSparseArray<int32> Array;
	TVoxelSparseArray<int32> VoxelArray;

	Run(
		"TSparseArray::Add (empty array)",
		[&]
		{
			Array.Empty();
			Array.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Array.Add(Num);
			}
		},
		"TVoxelSparseArray::Add (empty array)",
		[&]
		{
			VoxelArray.Empty();
			VoxelArray.Reserve(Num);
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelArray.Add(Num);
			}
		});
}

BENCHMARK
{
	TSparseArray<int32> Array;
	TVoxelSparseArray<int32> VoxelArray;

	Run(
		"TSparseArray::Add (full array, all removed)",
		[&]
		{
			Array.Empty();
			Array.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Array.Add(Index);
			}
			for (int32 Index = 0; Index < Num; Index++)
			{
				Array.RemoveAt(Index);
			}
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Array.Add(Num);
			}
		},
		"TVoxelSparseArray::Add (full array, all removed)",
		[&]
		{
			VoxelArray.Empty();
			VoxelArray.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				VoxelArray.Add(Index);
			}
			for (int32 Index = 0; Index < Num; Index++)
			{
				VoxelArray.RemoveAt(Index);
			}
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelArray.Add(Num);
			}
		});
}

BENCHMARK
{
	TSparseArray<int32> Array;
	TVoxelSparseArray<int32> VoxelArray;

	Run(
		"TSparseArray::RemoveAt",
		[&]
		{
			Array.Empty();
			Array.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Array.Add(Index);
			}
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				Array.RemoveAt(Run);
			}
		},
		"TVoxelSparseArray::RemoveAt",
		[&]
		{
			VoxelArray.Empty();
			VoxelArray.Reserve(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				VoxelArray.Add(Index);
			}
		},
		[&]
		{
			for (int32 Run = 0; Run < Num; Run++)
			{
				VoxelArray.RemoveAt(Run);
			}
		});
}

BENCHMARK
{
	for (const float Percent : TArray<float>{ 0, 0.25, 0.5, 0.75, 0.99 })
	{
		TSparseArray<int32> Array;
		TVoxelSparseArray<int32> VoxelArray;

		Array.Reserve(Num);
		VoxelArray.Reserve(Num);

		for (int32 Index = 0; Index < Num; Index++)
		{
			Array.Add(Index);
			VoxelArray.Add(Index);
		}

		const FRandomStream Stream(323234);
		uint64 ExpectedCount = 0;
		for (int32 Index = 0; Index < Num; Index++)
		{
			if (Stream.GetFraction() >= Percent)
			{
				ExpectedCount += Index;
			}
			else
			{
				Array.RemoveAt(Index);
				VoxelArray.RemoveAt(Index);
			}
		}

		RunBenchmark<1>(
			FString::Printf(TEXT("Iterating TSparseArray of size 1M %d%% empty"), int32(Percent * 100)),
			[&]
			{
				uint64 Count = 0;
				for (const int32 Index : Array)
				{
					Count += Index;
				}

				if (Count != ExpectedCount)
				{
					LOG_VOXEL(Fatal, "");
				}
			},
			FString::Printf(TEXT("Iterating TVoxelSparseArray of size 1M %d%% empty"), int32(Percent * 100)),
			[&]
			{
				uint64 Count = 0;
				for (const int32 Index : VoxelArray)
				{
					Count += Index;
				}

				if (Count != ExpectedCount)
				{
					LOG_VOXEL(Fatal, "");
				}
			});
	}
}

CUSTOM_BENCHMARK
{
	TSparseArray<int32> Array;
	TVoxelSparseArray<int32> VoxelArray;

	RunBenchmark<1>(
		"TSparseArray::Reserve(1M)",
		[&]
		{
			Array.Empty();
		},
		[&]
		{
			Array.Reserve(1000000);
		},
		"TVoxelSparseArray::Reserve(1M)",
		[&]
		{
			VoxelArray.Empty();
		},
		[&]
		{
			VoxelArray.Reserve(1000000);
		},
		"TSparseArray::Reserve creates a linked list, TVoxelSparseArray::Reserve is a memset");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}

#undef RUN_BENCHMARK
#undef BENCHMARK
#undef LOG
#endif