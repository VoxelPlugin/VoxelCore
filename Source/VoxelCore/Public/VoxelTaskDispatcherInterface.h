// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

extern VOXELCORE_API IVoxelTaskDispatcher* GVoxelGlobalTaskDispatcher;

class VOXELCORE_API FVoxelTaskDispatcherStrongRef
{
public:
	IVoxelTaskDispatcher& Dispatcher;

	explicit FVoxelTaskDispatcherStrongRef(IVoxelTaskDispatcher& Dispatcher);
	~FVoxelTaskDispatcherStrongRef();
};

class VOXELCORE_API FVoxelTaskDispatcherWeakRef
{
public:
	FVoxelTaskDispatcherWeakRef() = default;

	TUniquePtr<FVoxelTaskDispatcherStrongRef> Pin() const;

private:
	int32 Index = -1;
	int32 Serial = -1;

	friend IVoxelTaskDispatcher;
};

class VOXELCORE_API IVoxelTaskDispatcher
{
public:
	const bool bTrackPromisesCallstacks;

	explicit IVoxelTaskDispatcher(bool bTrackPromisesCallstacks);
	virtual ~IVoxelTaskDispatcher();

	VOXEL_COUNT_INSTANCES();

public:
	void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda);

public:
	void DumpToLog();

public:
	FORCEINLINE bool IsExiting() const
	{
		return bIsExiting.Get();
	}
	FORCEINLINE bool IsComplete() const
	{
		return
			NumPromises.Get() == 0 &&
			NumPendingTasks.Get() == 0;
	}
	FORCEINLINE int32 GetNumPromises() const
	{
		return NumPromises.Get();
	}
	FORCEINLINE int32 GetNumPendingTasks() const
	{
		return NumPendingTasks.Get();
	}

public:
	// Wrap another future created in a different task dispatcher
	// This ensures any continuation to this future won't leak to the other task dispatcher,
	// which would mess up task tracking
	FVoxelFuture Wrap(const FVoxelFuture& Other)
	{
		FVoxelPromise Promise(this);
		Promise.Set(Other);
		return Promise;
	}
	template<typename T>
	TVoxelFuture<T> Wrap(const TVoxelFuture<T>& Other)
	{
		TVoxelPromise<T> Promise(this);
		Promise.Set(Other);
		return Promise;
	}

	FORCEINLINE operator FVoxelTaskDispatcherWeakRef() const
	{
		return SelfWeakRef;
	}

private:
	FVoxelTaskDispatcherWeakRef SelfWeakRef;
	FVoxelCounter32_WithPadding NumStrongRefs;
	FVoxelCounter32_WithPadding NumPromises;
	FVoxelCounter32_WithPadding NumPendingTasks;
	FVoxelCounter32_WithPadding NumLaunchedTasks;
	TVoxelAtomic_WithPadding<bool> bIsExiting = false;

private:
	static constexpr int32 MaxLaunchedTasks = 256;
	using FTaskArray = TVoxelChunkedArray<TVoxelUniqueFunction<void()>, MaxLaunchedTasks * sizeof(TVoxelUniqueFunction<void()>) / 2>;

	FVoxelCriticalSection GameTasksCriticalSection;
	TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GameTasks_RequiresLock;

	FVoxelCriticalSection AsyncTasksCriticalSection;
	FTaskArray AsyncTasks_RequiresLock;

	void LaunchTasks();
	void LaunchTask(TVoxelUniqueFunction<void()> Task);

	void ProcessGameTasks(bool& bAnyTaskProcessed);

private:
	FVoxelCriticalSection CriticalSection;
	TVoxelSparseArray<FVoxelStackFrames> StackFrames_RequiresLock;
	TVoxelChunkedSparseArray<TSharedPtr<FVoxelPromiseState>> PromisesToKeepAlive_RequiresLock;

	friend FVoxelPromiseState;
	friend FVoxelTaskDispatcherWeakRef;
	friend FVoxelTaskDispatcherStrongRef;
	friend class FVoxelTaskDispatcherManager;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern VOXELCORE_API const uint32 GVoxelTaskDispatcherScopeTLS;

class VOXELCORE_API FVoxelTaskDispatcherScope
{
public:
	FORCEINLINE explicit FVoxelTaskDispatcherScope(IVoxelTaskDispatcher& Dispatcher)
		: Dispatcher(Dispatcher)
		, PreviousTLS(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS))
	{
		FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, &Dispatcher);
	}
	FORCEINLINE ~FVoxelTaskDispatcherScope()
	{
		checkVoxelSlow(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS) == &Dispatcher);
		FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, PreviousTLS);
	}

	FORCEINLINE static IVoxelTaskDispatcher& Get()
	{
		if (IVoxelTaskDispatcher* Dispatcher = static_cast<IVoxelTaskDispatcher*>(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS)))
		{
			return *Dispatcher;
		}

		return *GVoxelGlobalTaskDispatcher;
	}

public:
	// Call the lambda in the global task dispatcher scope, avoiding any task leak or weird dependencies
	template<
		typename LambdaType,
		typename T = LambdaReturnType_T<LambdaType>>
	static TVoxelFutureType<T> CallInGlobalScope(LambdaType Lambda)
	{
		return Get().Wrap(INLINE_LAMBDA
		{
			FVoxelTaskDispatcherScope Scope(*GVoxelGlobalTaskDispatcher);
			return Lambda();
		});
	}

private:
	IVoxelTaskDispatcher& Dispatcher;
	void* const PreviousTLS;
};