// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

extern VOXELCORE_API bool GVoxelNoAsync;

extern VOXELCORE_API FVoxelTaskContext* GVoxelGlobalTaskContext;
extern FVoxelTaskContext* GVoxelSynchronousTaskContext;

class VOXELCORE_API FVoxelTaskContextStrongRef
{
public:
	FVoxelTaskContext& Context;
	uint64 DebugId = 0;

	explicit FVoxelTaskContextStrongRef(FVoxelTaskContext& Context);
	~FVoxelTaskContextStrongRef();
};

class alignas(8) VOXELCORE_API FVoxelTaskContextWeakRef
{
public:
	FVoxelTaskContextWeakRef() = default;

	TUniquePtr<FVoxelTaskContextStrongRef> Pin() const;

	FORCEINLINE bool operator==(const FVoxelTaskContextWeakRef& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef<uint64>(Other);
	}

private:
	int32 Index = -1;
	int32 Serial = -1;

	friend FVoxelTaskContext;
};

class VOXELCORE_API FVoxelTaskContext
{
public:
	using FLambdaWrapper = TVoxelUniqueFunction<TVoxelUniqueFunction<void()>(TVoxelUniqueFunction<void()>)>;

	const FName Name;
	bool bSynchronous = false;
	bool bComputeTotalTime = false;
	bool bTrackPromisesCallstacks = false;
	int64 BulkDataTimestamp = MAX_int64;
	FLambdaWrapper LambdaWrapper;
	TVoxelAtomic<double> TotalTime;
	TSharedPtr<FVoxelDebugDrawGroup> DrawGroup;

private:
	static constexpr int32 MaxLaunchedTasks = 256;

public:
	static TSharedRef<FVoxelTaskContext> Create(FName Name, int32 MaxBackgroundTasks = MaxLaunchedTasks);
	virtual ~FVoxelTaskContext();
	UE_NONCOPYABLE(FVoxelTaskContext);

	VOXEL_COUNT_INSTANCES();

public:
	void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda);

	void CancelTasks();
	void DumpToLog() const;
	void FlushAllTasks() const;
	void FlushTasksUntil(TFunctionRef<bool()> Condition) const;

public:
	FORCEINLINE const TVoxelAtomic<bool>& GetShouldCancelTasksRef() const
	{
		return ShouldCancelTasks;
	}
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
	VOXEL_ATOMIC_PADDING;
	FVoxelCounter32 NumStrongRefs;
	VOXEL_ATOMIC_PADDING;
	FVoxelCounter32 NumPromises;
	VOXEL_ATOMIC_PADDING;
	FVoxelCounter32 NumPendingTasks;
	VOXEL_ATOMIC_PADDING;
	FVoxelCounter32 NumLaunchedTasks;
	VOXEL_ATOMIC_PADDING;
	FVoxelCounter32 NumRenderTasks;
	VOXEL_ATOMIC_PADDING;
	TVoxelAtomic<bool> ShouldCancelTasks = false;
	VOXEL_ATOMIC_PADDING;

private:
	using FTaskArray = TVoxelChunkedArray<TVoxelUniqueFunction<void()>, MaxLaunchedTasks * sizeof(TVoxelUniqueFunction<void()>) / 2>;

	int32 MaxBackgroundTasks = MaxLaunchedTasks;

	FVoxelCriticalSection GameTasksCriticalSection;
	TVoxelChunkedArray<TVoxelUniqueFunction<void()>> GameTasks_RequiresLock;

	FVoxelCriticalSection AsyncTasksCriticalSection;
	FTaskArray AsyncTasks_RequiresLock;

	bool bIsProcessingGameTasks = false;

	explicit FVoxelTaskContext(FName Name);

	void LaunchTasks();
	void LaunchTask(TVoxelUniqueFunction<void()> Task);

	void ProcessGameTasks(
		bool& bAnyTaskProcessed,
		TVoxelChunkedArray<TVoxelUniqueFunction<void()>>& OutGameTasksToDelete);

private:
	FVoxelCriticalSection CriticalSection;
	TVoxelMap<const FVoxelPromiseState*, FVoxelStackFrames> PromiseStateToStackFrames_RequiresLock;
	TVoxelChunkedSparseArray<TVoxelRefCountPtr<IVoxelPromiseState>> PromisesToKeepAlive_RequiresLock;

	void TrackPromise(const FVoxelPromiseState& PromiseState);
	void UntrackPromise(const FVoxelPromiseState& PromiseState);

private:
	FVoxelCriticalSection DebugDataCriticalSection;
	uint64 DebugCounter_RequiresLock = 0;
	TVoxelMap<uint64, FVoxelStackFrames> IdToStackFrame_RequiresLock;

	uint64 AddDebugFrame();
	void RemoveDebugFrame(uint64 DebugId);

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

private:
	FVoxelTaskContext& Context;
	void* const PreviousTLS;
};