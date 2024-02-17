// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelThreadPool.h"
#include "VoxelMemoryScope.h"
#include "Engine/Engine.h"
#include "HAL/RunnableThread.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, int32, GVoxelNumThreads, 2,
	"voxel.NumThreads",
	"The number of threads to use to process voxel tasks");

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

FVoxelThreadPool* GVoxelThreadPool = new FVoxelThreadPool();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelThreadPool::AddTask(TVoxelUniqueFunction<void()> Lambda)
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	Tasks_RequiresLock.Add(MoveTemp(Lambda));

	if (Tasks_RequiresLock.Num() < 32)
	{
		// Ensure a thread is awake if we don't have many tasks left
		// Over-conservative, but that's fine
		Event.Trigger();
	}
}

void FVoxelThreadPool::Initialize()
{
	TFunction<void()> Callback = [this]
	{
		bIsExiting.Set(true);

		VOXEL_SCOPE_LOCK(CriticalSection);
		Threads_RequiresLock.Reset();
		Tasks_RequiresLock.Reset();
	};

	FCoreDelegates::OnPreExit.AddLambda(Callback);
	FCoreDelegates::OnExit.AddLambda(Callback);
	GOnVoxelModuleUnloaded_DoCleanup.AddLambda(Callback);
}

void FVoxelThreadPool::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	if (IsExiting())
	{
		return;
	}

	GVoxelNumThreads = FMath::Clamp(GVoxelNumThreads, 1, 128);

	if (Threads_RequiresLock.Num() != GVoxelNumThreads)
	{
		AsyncBackgroundTask([this]
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			while (Threads_RequiresLock.Num() < GVoxelNumThreads)
			{
				Threads_RequiresLock.Add(MakeUnique<FThread>());
				Event.Trigger();
			}

			while (Threads_RequiresLock.Num() > GVoxelNumThreads)
			{
				Threads_RequiresLock.Pop();
			}
		});
	}

	const int32 CurrentNumTasks = NumTasks();

	if (!GVoxelHideTaskCount &&
		CurrentNumTasks > 0)
	{
		const FString Message = FString::Printf(TEXT("%d voxel tasks left using %d threads"), CurrentNumTasks, GVoxelNumThreads);
		GEngine->AddOnScreenDebugMessage(uint64(0x557D0C945D26), FApp::GetDeltaTime() * 1.5f, FColor::White, Message);

#if WITH_EDITOR
		extern UNREALED_API FLevelEditorViewportClient* GCurrentLevelEditingViewportClient;
		if (GCurrentLevelEditingViewportClient)
		{
			GCurrentLevelEditingViewportClient->SetShowStats(true);
		}
#endif
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

	Thread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(
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

	VOXEL_SCOPE_COUNTER_IMPL(true, "Process Tasks");

GetNextTask:
	if (bTimeToDie.Get())
	{
		return 0;
	}

	TVoxelUniqueFunction<void()> Task;
	{
		VOXEL_SCOPE_LOCK(GVoxelThreadPool->CriticalSection);

		if (GVoxelThreadPool->Tasks_RequiresLock.Num() == 0)
		{
			goto Wait;
		}

		Task = GVoxelThreadPool->Tasks_RequiresLock.Pop();
	}

	Task();

	goto GetNextTask;
}