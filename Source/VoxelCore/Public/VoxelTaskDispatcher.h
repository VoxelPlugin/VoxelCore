// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class IVoxelTaskDispatcher;

class VOXELCORE_API FVoxelTaskDispatcherKeepAliveRef
{
public:
	~FVoxelTaskDispatcherKeepAliveRef();

private:
	const TWeakPtr<IVoxelTaskDispatcher> WeakDispatcher;
	const int32 Index;

	FVoxelTaskDispatcherKeepAliveRef(
		const TSharedRef<IVoxelTaskDispatcher>& Dispatcher,
		const int32 Index)
		: WeakDispatcher(Dispatcher)
		, Index(Index)
	{
	}

	friend class IVoxelTaskDispatcher;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API IVoxelTaskDispatcher : public TSharedFromThis<IVoxelTaskDispatcher>
{
public:
	IVoxelTaskDispatcher() = default;
	virtual ~IVoxelTaskDispatcher() = default;

	virtual void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda) = 0;

	virtual bool IsExiting() const = 0;

	bool IsTrackingPromises() const
	{
		return NumPromisesPtr != nullptr;
	}

public:
#if VOXEL_DEBUG
	void CheckOwnsFuture(const FVoxelFuture& Future) const;
#else
	void CheckOwnsFuture(const FVoxelFuture& Future) const {}
#endif

public:
	// Wrap another future created in a different task dispatcher
	// This ensures any continuation to this future won't leak to the other task dispatcher,
	// which would mess up task tracking
	FVoxelFuture Wrap(
		const FVoxelFuture& Other,
		IVoxelTaskDispatcher& OtherDispatcher);

	template<typename T>
	TVoxelFuture<T> Wrap(
		const TVoxelFuture<T>& Other,
		IVoxelTaskDispatcher& OtherDispatcher);

protected:
	FVoxelCounter32* NumPromisesPtr = nullptr;

#if VOXEL_DEBUG
	FVoxelCriticalSection PromisesCriticalSection;
	TVoxelSet<FVoxelPromiseState*> Promises_RequiresLock;
#endif

private:
	FVoxelCriticalSection CriticalSection;
	TVoxelChunkedSparseArray<TSharedPtr<FVoxelPromiseState>> PromisesToKeepAlive_RequiresLock;

	TSharedRef<FVoxelTaskDispatcherKeepAliveRef> AddRef(const TSharedRef<FVoxelPromiseState>& Promise);

	friend class FVoxelPromiseState;
	friend class FVoxelTaskDispatcherKeepAliveRef;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelTaskDispatcherScope
{
public:
	explicit FVoxelTaskDispatcherScope(IVoxelTaskDispatcher& Dispatcher);
	~FVoxelTaskDispatcherScope();

	static IVoxelTaskDispatcher& Get();
	static IVoxelTaskDispatcher& GetGlobal();

	// Call the lambda in the global task dispatcher scope, avoiding any task leak or weird dependencies
	template<
		typename LambdaType,
		typename T = LambdaReturnType_T<LambdaType>>
	static TVoxelFutureType<T> CallInGlobalScope(LambdaType Lambda)
	{
		IVoxelTaskDispatcher& Dispatcher = Get();
		IVoxelTaskDispatcher& GlobalDispatcher = GetGlobal();

		TVoxelFutureType<T> Future;
		{
			FVoxelTaskDispatcherScope Scope(GlobalDispatcher);
			Future = Lambda();
		}
		return Dispatcher.Wrap(Future, GlobalDispatcher);
	}

private:
	IVoxelTaskDispatcher& Dispatcher;
	void* const PreviousTLS;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
TVoxelFuture<T> IVoxelTaskDispatcher::Wrap(
	const TVoxelFuture<T>& Other,
	IVoxelTaskDispatcher& OtherDispatcher)
{
	FVoxelTaskDispatcherScope Scope(*this);

	const TVoxelPromise<T> Promise;

	Wrap(static_cast<const FVoxelFuture&>(Other), OtherDispatcher).Then_AnyThread([this, Other, Promise]
	{
		checkVoxelSlow(&FVoxelTaskDispatcherScope::Get() == this);

		Promise.Set(Other.GetSharedValueChecked());
	});

	return Promise.GetFuture();
}