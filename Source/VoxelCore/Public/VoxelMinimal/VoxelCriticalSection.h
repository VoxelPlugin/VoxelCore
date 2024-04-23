// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"

struct FVoxelCacheLinePadding
{
public:
	FORCEINLINE FVoxelCacheLinePadding()
	{
	}
	FORCEINLINE FVoxelCacheLinePadding(const FVoxelCacheLinePadding&)
	{
	}
	FORCEINLINE FVoxelCacheLinePadding(FVoxelCacheLinePadding&&)
	{
	}
	FORCEINLINE FVoxelCacheLinePadding& operator=(const FVoxelCacheLinePadding&)
	{
		return *this;
	}
	FORCEINLINE FVoxelCacheLinePadding& operator=(FVoxelCacheLinePadding&&)
	{
		return *this;
	}

private:
	uint8 Padding[PLATFORM_CACHE_LINE_SIZE * 2];
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename StorageType>
class TVoxelCriticalSectionImpl
{
public:
	TVoxelCriticalSectionImpl() = default;

	// Allow copying for convenience, but don't copy the actual state
	FORCEINLINE TVoxelCriticalSectionImpl(const TVoxelCriticalSectionImpl&)
	{
	}
	FORCEINLINE TVoxelCriticalSectionImpl& operator=(const TVoxelCriticalSectionImpl&)
	{
		return *this;
	}

public:
	FORCEINLINE void Lock()
	{
		checkVoxelSlow(LockerThreadId.Get() != FPlatformTLS::GetCurrentThreadId());

		while (true)
		{
			if (Storage.bIsLocked.Exchange_ReturnOld(true, std::memory_order_acquire) == false)
			{
				break;
			}

			while (Storage.bIsLocked.Get(std::memory_order_relaxed) == true)
			{
				FPlatformProcess::Yield();
			}
		}

		checkVoxelSlow(LockerThreadId.Get() == 0);
		VOXEL_DEBUG_ONLY(LockerThreadId.Set(FPlatformTLS::GetCurrentThreadId()));
	}
	FORCEINLINE void Unlock()
	{
		checkVoxelSlow(LockerThreadId.Get() == FPlatformTLS::GetCurrentThreadId());
		VOXEL_DEBUG_ONLY(LockerThreadId.Set(0));

		Storage.bIsLocked.Set(false, std::memory_order_release);
	}

public:
	FORCEINLINE bool IsLocked() const
	{
		return Storage.bIsLocked.Get(std::memory_order_relaxed);
	}
	FORCEINLINE bool ShouldRecordStats() const
	{
		return IsLocked();
	}

private:
	StorageType Storage;
#if VOXEL_DEBUG
	TVoxelAtomic<uint32> LockerThreadId = 0;
#endif
};

struct Impl_FVoxelCriticalSection_Storage
{
	FVoxelCacheLinePadding PaddingA;
	TVoxelAtomic<bool> bIsLocked;
	FVoxelCacheLinePadding PaddingB;
};
using FVoxelCriticalSection = TVoxelCriticalSectionImpl<Impl_FVoxelCriticalSection_Storage>;

struct Impl_FVoxelCriticalSection_NoPadding_Storage
{
	TVoxelAtomic<bool> bIsLocked;
};
using FVoxelCriticalSection_NoPadding = TVoxelCriticalSectionImpl<Impl_FVoxelCriticalSection_NoPadding_Storage>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_SCOPE_LOCK(...) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).ShouldRecordStats(), "Lock " #__VA_ARGS__); \
		(__VA_ARGS__).Lock(); \
	} \
	ON_SCOPE_EXIT \
	{ \
		(__VA_ARGS__).Unlock(); \
	};