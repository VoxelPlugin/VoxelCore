// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMemoryScope.h"

const uint32 GVoxelMemoryTLS = FPlatformTLS::AllocTlsSlot();

#if VOXEL_DEBUG
#define CheckVoxelMemoryFunction() \
	thread_local bool _bIsRunning = false; \
	check(!_bIsRunning); \
	_bIsRunning = true; \
	ON_SCOPE_EXIT \
	{ \
		check(_bIsRunning); \
		_bIsRunning = false; \
	};
#else
#define CheckVoxelMemoryFunction()
#endif

#if ENABLE_VOXEL_ALLOCATOR && VOXEL_DEBUG
void FVoxelMemory::CheckIsVoxelAlloc(const void* Original)
{
	FVoxelMemoryScope::GetBlock(ConstCast(Original));
}
#endif

void* FVoxelMemory::MallocImpl(const SIZE_T Count, const uint32 Alignment)
{
	CheckVoxelMemoryFunction();
	checkVoxelSlow(Count > 0);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (!GVoxelLLMDisabled)
	{
		CheckVoxelLLMScope();
	}
#endif

	if (FVoxelMemoryScope* Scope = static_cast<FVoxelMemoryScope*>(FPlatformTLS::GetTlsValue(GVoxelMemoryTLS)))
	{
		return Scope->Malloc(Count, Alignment);
	}
	else
	{
		return FVoxelMemoryScope::StaticMalloc(Count, Alignment);
	}
}

void* FVoxelMemory::ReallocImpl(void* Original, const SIZE_T OriginalCount, const SIZE_T Count, const uint32 Alignment)
{
	CheckVoxelMemoryFunction();

	if (FVoxelMemoryScope* Scope = static_cast<FVoxelMemoryScope*>(FPlatformTLS::GetTlsValue(GVoxelMemoryTLS)))
	{
		return Scope->Realloc(Original, OriginalCount, Count, Alignment);
	}
	else
	{
		return FVoxelMemoryScope::StaticRealloc(Original, OriginalCount, Count, Alignment);
	}
}

void FVoxelMemory::FreeImpl(void* Original)
{
	CheckVoxelMemoryFunction();

	if (FVoxelMemoryScope* Scope = static_cast<FVoxelMemoryScope*>(FPlatformTLS::GetTlsValue(GVoxelMemoryTLS)))
	{
		return Scope->Free(Original);
	}
	else
	{
		FVoxelMemoryScope::StaticFree(Original);
	}
}

#undef CheckVoxelMemoryFunction