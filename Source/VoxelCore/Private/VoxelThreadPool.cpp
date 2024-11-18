// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelThreadPool.h"
#include "VoxelMemoryScope.h"
#include "VoxelCoreEditorSettings.h"
#include "VoxelGlobalTaskDispatcher.h"
#include "Engine/Engine.h"
#include "HAL/RunnableThread.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

VOXELCORE_API TOptional<int32> GVoxelNumThreadsOverride;

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, int32, GVoxelNumThreads, 2,
	"voxel.NumThreads",
	"The number of threads to use to process voxel tasks in game");

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, int32, GVoxelThreadPriority, 2,
	"voxel.ThreadPriority",
	"0: Normal"
	"1: AboveNormal"
	"2: BelowNormal"
	"3: Highest"
	"4: Lowest"
	"5: SlightlyBelowNormal"
	"6: TimeCritical");

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelHideTaskCount, false,
	"voxel.HideTaskCount",
	"");

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelVerboseTaskCount, false,
	"voxel.VerboseTaskCount",
	"");

FVoxelThreadPool* GVoxelThreadPool = new FVoxelThreadPool();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelTaskExecutor::TriggerThreads()
{
	VOXEL_FUNCTION_COUNTER();

	GVoxelThreadPool->Event.Trigger();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelThreadPool::~FVoxelThreadPool()
{
	// 2 for the global executors
	ensure(Executors_RequiresLock.Num() == 2);
}

int32 FVoxelThreadPool::NumTasks() const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(Executors_CriticalSection);

	int32 Result = 0;
	for (const IVoxelTaskExecutor* Executor : Executors_RequiresLock)
	{
		Result += Executor->NumTasks();
	}
	return Result;
}

void FVoxelThreadPool::AddExecutor(IVoxelTaskExecutor* Executor)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(Executors_CriticalSection);

	check(!Executors_RequiresLock.Contains(Executor));
	Executors_RequiresLock.Add(Executor);
}

void FVoxelThreadPool::RemoveExecutor(IVoxelTaskExecutor* Executor)
{
	VOXEL_FUNCTION_COUNTER();

	while (true)
	{
		{
			VOXEL_SCOPE_LOCK(Executors_CriticalSection);

			if (!ActiveExecutors_RequiresLock.Contains(Executor))
			{
				ensure(Executors_RequiresLock.Remove(Executor) == 1);
				return;
			}
		}

		FPlatformProcess::Yield();

		if (IsInGameThread())
		{
			VOXEL_SCOPE_LOCK(Executors_CriticalSection);

			// Avoid deadlock when graph executor is waiting on game thread
			Voxel::FlushGameTasks();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelThreadPool::Initialize()
{
	TFunction<void()> Callback = [this]
	{
		bIsExiting.Set(true);

		{
			VOXEL_SCOPE_LOCK(Threads_CriticalSection);
			Threads_RequiresLock.Reset();
		}
	};

	FCoreDelegates::OnPreExit.AddLambda(Callback);
	FCoreDelegates::OnExit.AddLambda(Callback);
	GOnVoxelModuleUnloaded_DoCleanup.AddLambda(Callback);

	GVoxelGlobalForegroundTaskDispatcher = MakeVoxelShared<FVoxelGlobalTaskDispatcher>(false);
	GVoxelGlobalBackgroundTaskDispatcher = MakeVoxelShared<FVoxelGlobalTaskDispatcher>(true);
}

void FVoxelThreadPool::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	if (IsExiting())
	{
		return;
	}

	int32 NumThreads = GVoxelNumThreads;

#if WITH_EDITOR
	if (GEditor &&
		!GEditor->PlayWorld &&
		!GIsPlayInEditorWorld &&
		!GetDefault<UVoxelCoreEditorSettings>()->bUseGameThreadingSettingsInEditor)
	{
		if (GetDefault<UVoxelCoreEditorSettings>()->bAutomaticallyScaleNumberOfThreads)
		{
			NumThreads = FPlatformMisc::NumberOfCores() - 2;
		}
		else
		{
			NumThreads = GetDefault<UVoxelCoreEditorSettings>()->NumberOfThreadsInEditor;
		}
	}
#endif

	if (GVoxelNumThreadsOverride.IsSet())
	{
		NumThreads = GVoxelNumThreadsOverride.GetValue();
	}

	NumThreads = FMath::Clamp(NumThreads, 1, 128);

	if (Threads_RequiresLock.Num() != NumThreads)
	{
		Voxel::AsyncTask_SkipDispatcher([this, NumThreads]
		{
			VOXEL_SCOPE_LOCK(Threads_CriticalSection);

			while (Threads_RequiresLock.Num() < NumThreads)
			{
				Threads_RequiresLock.Add(MakeUnique<FThread>());
				Event.Trigger();
			}

			while (Threads_RequiresLock.Num() > NumThreads)
			{
				Threads_RequiresLock.Pop();
			}
		});
	}

	const int32 CurrentNumTasks = NumTasks();

	if (!GVoxelHideTaskCount)
	{
		const auto ShowStats = [&](const uint64 Id, const FString& Text)
		{
			GEngine->AddOnScreenDebugMessage(Id, 0.1f, FColor::White, Text);

#if WITH_EDITOR
			extern UNREALED_API FLevelEditorViewportClient* GCurrentLevelEditingViewportClient;
			if (GCurrentLevelEditingViewportClient)
			{
				GCurrentLevelEditingViewportClient->SetShowStats(true);
			}
#endif
		};

		if (CurrentNumTasks > 0)
		{
			ShowStats(uint64(0x557D0C945D26), FString::Printf(TEXT("%d voxel tasks left using %d threads"), CurrentNumTasks, NumThreads));
		}
		else
		{
			ShowStats(uint64(0x557D0C945D26), {});
		}

		if (GVoxelVerboseTaskCount)
		{
			const int32 NumForegroundTasks = GVoxelGlobalForegroundTaskDispatcher->NumTasks_Actual();
			const int32 NumBackgroundTasks = GVoxelGlobalBackgroundTaskDispatcher->NumTasks_Actual();

			ShowStats(uint64(0x322D765FAC1E), FString::Printf(TEXT("%d foreground voxel tasks left using %d threads"), NumForegroundTasks, NumThreads));
			ShowStats(uint64(0xC2C1E182DD07), FString::Printf(TEXT("%d background voxel tasks left using %d threads"), NumBackgroundTasks, NumThreads));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelThreadPool::FThread::FThread()
{
	UE::Trace::ThreadGroupBegin(TEXT("VoxelThreadPool"));

	static int32 ThreadIndex = 0;
	const FString Name = FString::Printf(TEXT("Voxel Thread %d"), ThreadIndex++);

	Thread = TUniquePtr<FRunnableThread>(FForkProcessHelper::CreateForkableThread(
		this,
		*Name,
		1024 * 1024,
		EThreadPriority(FMath::Clamp(GVoxelThreadPriority, 0, 6)),
		FPlatformAffinity::GetPoolThreadMask()));

	UE::Trace::ThreadGroupEnd();
}

FVoxelThreadPool::FThread::~FThread()
{
	VOXEL_FUNCTION_COUNTER();

	// Tell the thread it needs to die
	bTimeToDie.Set(true);
	// Trigger the thread so that it will come out of the wait state if
	// it isn't actively doing work
	GVoxelThreadPool->Event.Trigger();
	// Kill (but wait for thread to finish)
	Thread->Kill(true);
	// Delete
	Thread.Reset();
}

uint32 FVoxelThreadPool::FThread::Run()
{
	VOXEL_LLM_SCOPE();

	const TUniquePtr<FVoxelMemoryScope> MemoryScope = MakeUnique<FVoxelMemoryScope>();

Wait:
	if (bTimeToDie.Get())
	{
		return 0;
	}

	MemoryScope->Clear();

	if (!GVoxelThreadPool->Event.Wait(10))
	{
		goto Wait;
	}

GetNextTask:
	if (bTimeToDie.Get())
	{
		return 0;
	}

	bool bAnyExecuted = false;
	{
		int32 ExecutorIndex = 0;
		while (true)
		{
			IVoxelTaskExecutor* Executor;
			{
				VOXEL_SCOPE_LOCK(GVoxelThreadPool->Executors_CriticalSection);

				if (!GVoxelThreadPool->Executors_RequiresLock.IsValidIndex(ExecutorIndex))
				{
					break;
				}

				Executor = GVoxelThreadPool->Executors_RequiresLock[ExecutorIndex];
				ExecutorIndex++;

				GVoxelThreadPool->ActiveExecutors_RequiresLock.Add(Executor);
			}

			bAnyExecuted |= Executor->TryExecuteTasks_AnyThread();

			{
				VOXEL_SCOPE_LOCK(GVoxelThreadPool->Executors_CriticalSection);
				ensure(GVoxelThreadPool->ActiveExecutors_RequiresLock.RemoveSingleSwap(Executor, EAllowShrinking::No));
			}
		}
	}

	if (!bAnyExecuted)
	{
		goto Wait;
	}

	goto GetNextTask;
}