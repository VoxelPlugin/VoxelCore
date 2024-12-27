// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"

namespace FVoxelUtilities
{
	FORCEINLINE bool TryLockAtomic(TVoxelAtomic<bool>& bIsLocked)
	{
		return bIsLocked.Exchange_ReturnOld(true, std::memory_order_acquire) == false;
	}
	FORCEINLINE void LockAtomic(TVoxelAtomic<bool>& bIsLocked)
	{
		while (true)
		{
			if (TryLockAtomic(bIsLocked))
			{
				break;
			}

			while (bIsLocked.Get(std::memory_order_relaxed) == true)
			{
				FPlatformProcess::Yield();
			}
		}
	}
	FORCEINLINE void UnlockAtomic(TVoxelAtomic<bool>& bIsLocked)
	{
		bIsLocked.Set(false, std::memory_order_release);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

		FVoxelUtilities::LockAtomic(Storage.bIsLocked);

		checkVoxelSlow(LockerThreadId.Get() == 0);
		VOXEL_DEBUG_ONLY(LockerThreadId.Set(FPlatformTLS::GetCurrentThreadId()));
	}
	FORCEINLINE bool TryLock()
	{
		checkVoxelSlow(LockerThreadId.Get() != FPlatformTLS::GetCurrentThreadId());

		if (!FVoxelUtilities::TryLockAtomic(Storage.bIsLocked))
		{
			return false;
		}

		checkVoxelSlow(LockerThreadId.Get() == 0);
		VOXEL_DEBUG_ONLY(LockerThreadId.Set(FPlatformTLS::GetCurrentThreadId()));

		return true;
	}

	FORCEINLINE void Unlock()
	{
		checkVoxelSlow(LockerThreadId.Get() == FPlatformTLS::GetCurrentThreadId());
		VOXEL_DEBUG_ONLY(LockerThreadId.Set(0));

		FVoxelUtilities::UnlockAtomic(Storage.bIsLocked);
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

#define VOXEL_SCOPE_LOCK_ATOMIC(...) \
	{ \
		VOXEL_SCOPE_COUNTER_COND((__VA_ARGS__).Get(std::memory_order_relaxed), "Lock " #__VA_ARGS__); \
		FVoxelUtilities::LockAtomic(__VA_ARGS__); \
	} \
	ON_SCOPE_EXIT \
	{ \
		FVoxelUtilities::UnlockAtomic(__VA_ARGS__); \
	};