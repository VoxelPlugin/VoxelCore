// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API IVoxelTaskDispatcher
{
public:
	IVoxelTaskDispatcher() = default;
	virtual ~IVoxelTaskDispatcher() = default;

	FORCEINLINE int32 NumPromises() const
	{
		return PrivateNumPromises.Get();
	}

	virtual void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda) = 0;

private:
	FVoxelCounter32 PrivateNumPromises;

	friend class FVoxelPromiseState;
};

class VOXELCORE_API FVoxelTaskDispatcherScope
{
public:
	explicit FVoxelTaskDispatcherScope(const TSharedRef<IVoxelTaskDispatcher>& Dispatcher);
	~FVoxelTaskDispatcherScope();

	static TSharedPtr<IVoxelTaskDispatcher> Get();

private:
	const TSharedRef<IVoxelTaskDispatcher>& Dispatcher;
	void* const PreviousTLS;
};

class VOXELCORE_API FVoxelDefaultTaskDispatcher : public IVoxelTaskDispatcher
{
public:
	FVoxelDefaultTaskDispatcher() = default;

	//~ Begin IVoxelTaskDispatcher Interface
	virtual void Dispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda) override;
	//~ End IVoxelTaskDispatcher Interface

public:
	static void StaticDispatch(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda);
};