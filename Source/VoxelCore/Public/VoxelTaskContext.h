// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

extern VOXELCORE_API FVoxelTaskContext* GVoxelGlobalTaskContext;

class VOXELCORE_API FVoxelTaskContextStrongRef
{
public:
	FVoxelTaskContext& Context;

	explicit FVoxelTaskContextStrongRef(FVoxelTaskContext& Context);
	~FVoxelTaskContextStrongRef();
};

class VOXELCORE_API FVoxelTaskContextWeakRef
{
public:
	FVoxelTaskContextWeakRef() = default;

	TUniquePtr<FVoxelTaskContextStrongRef> Pin() const;

private:
	int32 Index = -1;
	int32 Serial = -1;

	friend FVoxelTaskContext;
};

class VOXELCORE_API FVoxelTaskContext
{
public:
	const bool bCanCancelTasks;
	const bool bTrackPromisesCallstacks;

	FVoxelTaskContext(
		bool bCanCancelTasks,
		bool bTrackPromisesCallstacks);
	virtual ~FVoxelTaskContext();

	VOXEL_COUNT_INSTANCES();

public:
	void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda);

	void FlushTasks();
	void DumpToLog();

public:
	FORCEINLINE bool IsCancellingTasks() const
	{
		return ShouldCancelTasks.Get(std::memory_order_relaxed);
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
	// Wrap another future created in a different task context
	// This ensures any continuation to this future won't leak to the other context,
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

	FORCEINLINE operator FVoxelTaskContextWeakRef() const
	{
		return SelfWeakRef;
	}

private:
	FVoxelTaskContextWeakRef SelfWeakRef;
	FVoxelCounter32_WithPadding NumStrongRefs;
	FVoxelCounter32_WithPadding NumPromises;
	FVoxelCounter32_WithPadding NumPendingTasks;
	FVoxelCounter32_WithPadding NumLaunchedTasks;
	FVoxelCounter32_WithPadding NumRenderTasks;
	TVoxelAtomic_WithPadding<bool> ShouldCancelTasks = false;

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
	friend FVoxelTaskContextWeakRef;
	friend FVoxelTaskContextStrongRef;
	friend class FVoxelTaskContextTicker;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern VOXELCORE_API const uint32 GVoxelTaskScopeTLS;

class VOXELCORE_API FVoxelTaskScope
{
public:
	FORCEINLINE explicit FVoxelTaskScope(FVoxelTaskContext& Context)
		: Context(Context)
		, PreviousTLS(FPlatformTLS::GetTlsValue(GVoxelTaskScopeTLS))
	{
		FPlatformTLS::SetTlsValue(GVoxelTaskScopeTLS, &Context);
	}
	FORCEINLINE ~FVoxelTaskScope()
	{
		checkVoxelSlow(FPlatformTLS::GetTlsValue(GVoxelTaskScopeTLS) == &Context);
		FPlatformTLS::SetTlsValue(GVoxelTaskScopeTLS, PreviousTLS);
	}

	FORCEINLINE static FVoxelTaskContext& GetContext()
	{
		if (FVoxelTaskContext* Context = static_cast<FVoxelTaskContext*>(FPlatformTLS::GetTlsValue(GVoxelTaskScopeTLS)))
		{
			return *Context;
		}

		return *GVoxelGlobalTaskContext;
	}

public:
	// Call the lambda in the global task context, avoiding any task leak or weird dependencies
	template<
		typename LambdaType,
		typename T = LambdaReturnType_T<LambdaType>>
	static TVoxelFutureType<T> CallInGlobalContext(LambdaType Lambda)
	{
		return GetContext().Wrap(INLINE_LAMBDA
		{
			FVoxelTaskScope Scope(*GVoxelGlobalTaskContext);
			return Lambda();
		});
	}

private:
	FVoxelTaskContext& Context;
	void* const PreviousTLS;
};