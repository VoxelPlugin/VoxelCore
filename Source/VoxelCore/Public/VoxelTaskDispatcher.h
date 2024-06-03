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

protected:
	FVoxelCounter32* NumPromisesPtr = nullptr;

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

private:
	IVoxelTaskDispatcher& Dispatcher;
	void* const PreviousTLS;
};