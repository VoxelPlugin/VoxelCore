// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "HAL/Runnable.h"

extern VOXELCORE_API int32 GVoxelNumThreads;

class VOXELCORE_API IVoxelTaskExecutor
{
public:
	IVoxelTaskExecutor() = default;
	virtual ~IVoxelTaskExecutor() = default;

	// Return number of executed tasks
	virtual void FlushGameThreadTasks() = 0;
	virtual bool ExecuteTasks_AnyThread() = 0;
	virtual int32 NumTasks() const = 0;

public:
	static void TriggerThreads();
};

class VOXELCORE_API FVoxelThreadPool : public FVoxelSingleton
{
public:
	virtual ~FVoxelThreadPool() override;

	FORCEINLINE bool IsExiting() const
	{
		return bIsExiting.Get();
	}

	int32 NumTasks() const;

	void AddExecutor(IVoxelTaskExecutor* Executor);
	void RemoveExecutor(IVoxelTaskExecutor* Executor);

public:
	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override;
	virtual void Tick() override;
	//~ End FVoxelSingleton Interface

private:
	class FThread : public FRunnable
	{
	public:
		FThread();
		virtual ~FThread() override;

		//~ Begin FRunnable Interface
		virtual uint32 Run() override;
		//~ End FRunnable Interface

	private:
		TVoxelAtomic<bool> bTimeToDie = false;
		TUniquePtr<FRunnableThread> Thread;
	};

	FEvent& Event = *FPlatformProcess::GetSynchEventFromPool();
	TVoxelAtomic<bool> bIsExiting = false;

	FVoxelCriticalSection Threads_CriticalSection;
	TVoxelArray<TUniquePtr<FThread>> Threads_RequiresLock;

	mutable FVoxelCriticalSection Executors_CriticalSection;
	TVoxelArray<IVoxelTaskExecutor*> Executors_RequiresLock;
	TVoxelArray<IVoxelTaskExecutor*> ActiveExecutors_RequiresLock;

	friend IVoxelTaskExecutor;
};
extern VOXELCORE_API FVoxelThreadPool* GVoxelThreadPool;