// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "HAL/Runnable.h"

extern VOXELCORE_API int32 GVoxelNumThreads;
extern VOXELCORE_API int32 GVoxelThreadPriority;
extern VOXELCORE_API bool GVoxelHideTaskCount;

class VOXELCORE_API FVoxelThreadPool : public FVoxelSingleton
{
public:
	FORCEINLINE int32 NumTasks() const
	{
		// Just peeking num, no need to lock
		return Tasks_RequiresLock.Num();
	}
	FORCEINLINE bool IsExiting() const
	{
		return bIsExiting.Get();
	}
	void AddTask(TVoxelUniqueFunction<void()> Lambda);
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

	FVoxelCriticalSection CriticalSection;
	TVoxelArray<TUniquePtr<FThread>> Threads_RequiresLock;
	TVoxelArray<TVoxelUniqueFunction<void()>> Tasks_RequiresLock;
};
extern FVoxelThreadPool* GVoxelThreadPool;