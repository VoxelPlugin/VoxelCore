// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Async/ParallelFor.h"

#if 0
VOXEL_RUN_ON_STARTUP_GAME()
{
	static FVoxelCounter32 Counter;

	const TFunction<void(int32)> Task = [&](const int32 Index)
	{
		Counter.Add(Index);
	};
	const auto Task2 = [&](const int32 Index)
	{
		Counter.Add(Index);
	};

	while (true)
	{
		{
			VOXEL_LOG_SCOPE_STATS("Stock");

			ParallelFor(1024 * 1024, Task);
		}

		{
			VOXEL_LOG_SCOPE_STATS("Voxel");

			Voxel::ParallelFor(1024 * 1024, Task);
		}

		{
			VOXEL_LOG_SCOPE_STATS("Stock 2");

			ParallelFor(1024 * 1024, Task2);
		}

		{
			VOXEL_LOG_SCOPE_STATS("Voxel 2");

			Voxel::ParallelFor(1024 * 1024, Task2);
		}
	}
}
#endif

int32 Voxel::Internal::GetMaxNumThreads()
{
	// See ParallelForImpl::GetNumberOfThreadTasks

	int32 Result = 0;
	if (FApp::ShouldUseThreadingForPerformance() ||
		FForkProcessHelper::IsForkedMultithreadInstance())
	{
		Result = LowLevelTasks::FScheduler::Get().GetNumWorkers();
	}

	if (!LowLevelTasks::FScheduler::Get().IsWorkerThread())
	{
		Result++;
	}

	return FMath::Clamp(Result, 1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
}

void Voxel::Internal::ParallelFor(
	const int64 Num,
	const TFunctionRef<void(int64 StartIndex, int64 EndIndex)> Lambda)
{
	VOXEL_FUNCTION_COUNTER_NUM(Num);
	checkVoxelSlow(Num >= 0);

	if (Num == 0)
	{
		return;
	}

	int64 NumThreads = FMath::Clamp<int64>(GetMaxNumThreads(), 1, Num);

	if (NumThreads == 1)
	{
		VOXEL_SCOPE_COUNTER_NUM("Voxel::ParallelFor", Num);
		Lambda(0, Num);
		return;
	}

	const int64 ElementsPerThreads = FVoxelUtilities::DivideCeil_Positive(Num, NumThreads);

	// Update NumThreads: if Num = 100 and NumThreads = 49, ElementsPerThreads would be 3 => we only need 34 threads
	NumThreads = FVoxelUtilities::DivideCeil_Positive(Num, ElementsPerThreads);

	TVoxelOptional<ETaskTag> TaskTag;
	LowLevelTasks::ETaskPriority Priority;
	{
		const ETaskTag HighPriorityTasks =
			ETaskTag::EGameThread |
			ETaskTag::ERenderingThread |
			ETaskTag::ERhiThread;

		if (EnumHasAnyFlags(FTaskTagScope::GetCurrentTag(), HighPriorityTasks))
		{
			TaskTag = (FTaskTagScope::GetCurrentTag() & HighPriorityTasks) | ETaskTag::EParallelThread;
			Priority = LowLevelTasks::ETaskPriority::High;
		}
		else
		{
			Priority = LowLevelTasks::ETaskPriority::BackgroundNormal;
		}
	};

	const TSharedRef<FVoxelCounter32> NumTasksStarted = MakeShared<FVoxelCounter32>();
	FVoxelCounter32 NumTasksDone;

	const auto TryProcessTask = [NumTasksStarted, NumThreads, ElementsPerThreads, Num, &Lambda, &NumTasksDone]
	{
		const int32 TaskIndex = NumTasksStarted->Increment_ReturnOld();
		if (TaskIndex >= NumThreads)
		{
			return false;
		}

		{
			const int64 StartIndex = TaskIndex * ElementsPerThreads;
			const int64 EndIndex = FMath::Min((TaskIndex + 1) * ElementsPerThreads, Num);

			VOXEL_SCOPE_COUNTER_FORMAT("Task %d", TaskIndex);
			VOXEL_SCOPE_COUNTER_NUM("Voxel::ParallelFor", EndIndex - StartIndex);
			Lambda(StartIndex, EndIndex);
		}

		NumTasksDone.Increment();
		return true;
	};

	{
		VOXEL_SCOPE_COUNTER("Start threads");

		for (int32 ThreadIndex = 1; ThreadIndex < NumThreads; ThreadIndex++)
		{
			LowLevelTasks::FTask* Task = new LowLevelTasks::FTask();
			Task->Init(
				TEXT("Voxel.ParallelFor"),
				Priority,
				[TaskTag, TryProcessTask, TaskPtr = TUniquePtr<LowLevelTasks::FTask>{ Task }]
				{
					TVoxelOptional<UE_506_SWITCH(FOptionalTaskTagScope, FTaskTagScope)> Scope;
					if (TaskTag.IsSet())
					{
						Scope.Emplace(TaskTag.GetValue());
					}

					TryProcessTask();
				});

			const bool bSuccess = LowLevelTasks::TryLaunch(
				*Task,
				LowLevelTasks::EQueuePreference::GlobalQueuePreference);

			checkVoxelSlow(bSuccess);
		}
	}

	// Tricky: process as many tasks as we can inline,
	// other worker threads could be stuck waiting on this thread
	while (true)
	{
		if (!TryProcessTask())
		{
			break;
		}
	}

	if (NumTasksDone.Get() == NumThreads)
	{
		return;
	}

	FVoxelUtilities::WaitFor([&]
	{
		return NumTasksDone.Get() == NumThreads;
	});
}