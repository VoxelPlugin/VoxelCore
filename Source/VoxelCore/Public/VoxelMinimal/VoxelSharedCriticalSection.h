// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelCriticalSection.h"

struct FVoxelSharedCriticalSectionState
{
	uint16 NumReaders = 0;
	uint16 NumWriters = 0;
};

template<typename StorageType>
class TVoxelSharedCriticalSectionImpl
{
public:
	using FState = FVoxelSharedCriticalSectionState;

	TVoxelSharedCriticalSectionImpl() = default;

	// Allow copying for convenience, but don't copy the actual state
	FORCEINLINE TVoxelSharedCriticalSectionImpl(const TVoxelSharedCriticalSectionImpl&)
	{
	}
	FORCEINLINE TVoxelSharedCriticalSectionImpl& operator=(const TVoxelSharedCriticalSectionImpl&)
	{
		return *this;
	}

public:
	FORCEINLINE bool TryReadLock()
	{
		FState OldState = Storage.State.Get(std::memory_order_relaxed);

		if (OldState.NumWriters > 0)
		{
			return false;
		}

		FState NewState = OldState;
		NewState.NumReaders++;

		if (!Storage.State.CompareExchangeStrong(OldState, NewState))
		{
			return false;
		}

		checkVoxelSlow(Storage.State.Get().NumReaders > 0);
		checkVoxelSlow(Storage.State.Get().NumWriters == 0);

		return true;
	}
	FORCEINLINE void ReadLock()
	{
		FState OldState = Storage.State.Get(std::memory_order_relaxed);

	TryLock:
		while (OldState.NumWriters > 0)
		{
			FPlatformProcess::Yield();
			OldState = Storage.State.Get(std::memory_order_relaxed);
		}

		FState NewState = OldState;
		NewState.NumReaders++;

		if (!Storage.State.CompareExchangeStrong(OldState, NewState))
		{
			goto TryLock;
		}

		checkVoxelSlow(Storage.State.Get().NumReaders > 0);
		checkVoxelSlow(Storage.State.Get().NumWriters == 0);
	}
	FORCEINLINE void ReadUnlock()
	{
		Storage.State.Apply([&](FState State)
		{
			checkVoxelSlow(State.NumReaders > 0);
			checkVoxelSlow(State.NumWriters == 0);

			State.NumReaders--;
			return State;
		});
	}

public:
	FORCEINLINE bool TryWriteLock()
	{
		FState OldState = Storage.State.Get(std::memory_order_relaxed);

		if (OldState.NumReaders > 0 ||
			OldState.NumWriters > 0)
		{
			return false;
		}

		FState NewState = OldState;
		NewState.NumWriters++;

		if (!Storage.State.CompareExchangeStrong(OldState, NewState))
		{
			return false;
		}

		checkVoxelSlow(Storage.State.Get().NumReaders == 0);
		checkVoxelSlow(Storage.State.Get().NumWriters == 1);

		return true;
	}
	FORCEINLINE void WriteLock()
	{
		FState OldState = Storage.State.Get(std::memory_order_relaxed);

	TryLock:
		while (
			OldState.NumReaders > 0 ||
			OldState.NumWriters > 0)
		{
			FPlatformProcess::Yield();
			OldState = Storage.State.Get(std::memory_order_relaxed);
		}

		FState NewState = OldState;
		NewState.NumWriters++;

		if (!Storage.State.CompareExchangeStrong(OldState, NewState))
		{
			goto TryLock;
		}

		checkVoxelSlow(Storage.State.Get().NumReaders == 0);
		checkVoxelSlow(Storage.State.Get().NumWriters == 1);
	}
	FORCEINLINE void WriteUnlock()
	{
		Storage.State.Apply([&](FState State)
		{
			checkVoxelSlow(State.NumReaders == 0);
			checkVoxelSlow(State.NumWriters == 1);

			State.NumWriters--;
			return State;
		});
	}

public:
	FORCEINLINE bool IsLocked_Read() const
	{
		const FState State = Storage.State.Get(std::memory_order::relaxed);
		return
			State.NumReaders > 0 ||
			State.NumWriters > 0;
	}
	FORCEINLINE bool IsLocked_Write() const
	{
		const FState State = Storage.State.Get(std::memory_order::relaxed);
		return State.NumWriters > 0;
	}

public:
	FORCEINLINE bool ShouldRecordStats_Read() const
	{
		return IsLocked_Write();
	}
	FORCEINLINE bool ShouldRecordStats_Write() const
	{
		return IsLocked_Read();
	}

private:
	StorageType Storage;
};

struct Impl_FVoxelSharedCriticalSection_Storage
{
	FVoxelCacheLinePadding PaddingA;
	TVoxelAtomic<FVoxelSharedCriticalSectionState> State;
	FVoxelCacheLinePadding PaddingB;
};
using FVoxelSharedCriticalSection = TVoxelSharedCriticalSectionImpl<Impl_FVoxelSharedCriticalSection_Storage>;

struct Impl_FVoxelSharedCriticalSection_NoPadding_Storage
{
	TVoxelAtomic<FVoxelSharedCriticalSectionState> State;
};
using FVoxelSharedCriticalSection_NoPadding = TVoxelSharedCriticalSectionImpl<Impl_FVoxelSharedCriticalSection_NoPadding_Storage>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_SCOPE_READ_LOCK(...) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).ShouldRecordStats_Read(), "ReadLock " #__VA_ARGS__); \
		(__VA_ARGS__).ReadLock(); \
	} \
	ON_SCOPE_EXIT \
	{ \
		(__VA_ARGS__).ReadUnlock(); \
	};

#define VOXEL_SCOPE_WRITE_LOCK(...) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).ShouldRecordStats_Write(), "WriteLock " #__VA_ARGS__); \
		(__VA_ARGS__).WriteLock(); \
	} \
	ON_SCOPE_EXIT \
	{ \
		(__VA_ARGS__).WriteUnlock(); \
	};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_SCOPE_READ_LOCK_COND(Cond, ...) \
	const bool VOXEL_APPEND_LINE(__bShouldLock) = Cond; \
	if (VOXEL_APPEND_LINE(__bShouldLock)) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).ShouldRecordStats_Read(), "ReadLock " #__VA_ARGS__); \
		(__VA_ARGS__).ReadLock(); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_APPEND_LINE(__bShouldLock)) \
		{ \
			(__VA_ARGS__).ReadUnlock(); \
		} \
	};

#define VOXEL_SCOPE_WRITE_LOCK_COND(Cond, ...) \
	const bool VOXEL_APPEND_LINE(__bShouldLock) = Cond; \
	if (VOXEL_APPEND_LINE(__bShouldLock)) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).ShouldRecordStats_Write(), "WriteLock " #__VA_ARGS__); \
		(__VA_ARGS__).WriteLock(); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_APPEND_LINE(__bShouldLock)) \
		{ \
			(__VA_ARGS__).WriteUnlock(); \
		} \
	};