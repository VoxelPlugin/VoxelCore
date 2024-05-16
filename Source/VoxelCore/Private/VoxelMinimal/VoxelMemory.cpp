// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMemoryScope.h"

#if VOXEL_ALLOC_DEBUG
constexpr int32 GVoxelMinAllocationSize = 48;

bool GVoxelMallocDisableChecks = true;
thread_local bool GVoxelMallocIsAllowed = true;
thread_local bool GVoxelReallocIsAllowed = false;

bool EnterVoxelAllowMallocScope()
{
	const bool bBackup = GVoxelMallocIsAllowed;
	GVoxelMallocIsAllowed = true;
	return bBackup;
}

void ExitVoxelAllowMallocScope(const bool bBackup)
{
	checkVoxelSlow(GVoxelMallocIsAllowed);
	GVoxelMallocIsAllowed = bBackup;
}

bool EnterVoxelAllowReallocScope()
{
	const bool bBackup = GVoxelReallocIsAllowed;
	GVoxelReallocIsAllowed = true;
	return bBackup;
}

void ExitVoxelAllowReallocScope(const bool bBackup)
{
	checkVoxelSlow(GVoxelReallocIsAllowed);
	GVoxelReallocIsAllowed = bBackup;
}

class FVoxelDebugMalloc : public FMalloc
{
public:
	FMalloc& Base;

	explicit FVoxelDebugMalloc(FMalloc& Base)
		: Base(Base)
	{
	}

public:
	virtual void* Malloc(const SIZE_T Count, const uint32 Alignment) override
	{
		if (!GVoxelMallocDisableChecks &&
			!GVoxelMallocIsAllowed &&
			(Count > GVoxelMinAllocationSize || Alignment > 16))
		{
			UE_DEBUG_BREAK();
		}
		return Base.Malloc(Count, Alignment);
	}
	virtual void* TryMalloc(const SIZE_T Count, const uint32 Alignment) override
	{
		if (!GVoxelMallocDisableChecks &&
			!GVoxelMallocIsAllowed &&
			(Count > GVoxelMinAllocationSize || Alignment > 16))
		{
			UE_DEBUG_BREAK();
		}
		return Base.TryMalloc(Count, Alignment);
	}
	virtual void* Realloc(void* Original, const SIZE_T Count, const uint32 Alignment) override
	{
		if (!GVoxelMallocDisableChecks &&
			!GVoxelMallocIsAllowed &&
			(Count > GVoxelMinAllocationSize || Alignment > 16))
		{
			UE_DEBUG_BREAK();
		}
		return Base.Realloc(Original, Count, Alignment);
	}
	virtual void* TryRealloc(void* Original, const SIZE_T Count, const uint32 Alignment) override
	{
		if (!GVoxelMallocDisableChecks &&
			!GVoxelMallocIsAllowed &&
			(Count > GVoxelMinAllocationSize || Alignment > 16))
		{
			UE_DEBUG_BREAK();
		}
		return Base.TryRealloc(Original, Count, Alignment);
	}
	virtual void Free(void* Original) override
	{
#if VOXEL_DEBUG && 0
		extern FVoxelCriticalSection GVoxelValidAllocationsCriticalSection;
		if (!GVoxelValidAllocationsCriticalSection.IsLockedByThisThread_Debug())
		{
			extern TSet<void*> GVoxelValidAllocations;

			VOXEL_SCOPE_LOCK(GVoxelValidAllocationsCriticalSection);
			check(!GVoxelValidAllocations.Contains(Original));
		}
#endif

		SIZE_T Count = 0;
		ensure(Base.GetAllocationSize(Original, Count));

		if (!GVoxelMallocDisableChecks &&
			!GVoxelMallocIsAllowed &&
			Count > GVoxelMinAllocationSize)
		{
			UE_DEBUG_BREAK();
		}
		Base.Free(Original);
	}

	virtual SIZE_T QuantizeSize(const SIZE_T Count, const uint32 Alignment) override { return Base.QuantizeSize(Count, Alignment); }
	virtual bool GetAllocationSize(void* Original, SIZE_T& SizeOut) override { return Base.GetAllocationSize(Original, SizeOut); }
	virtual void Trim(const bool bTrimThreadCaches) override { Base.Trim(bTrimThreadCaches); }
	virtual void SetupTLSCachesOnCurrentThread() override { Base.SetupTLSCachesOnCurrentThread(); }
	virtual void ClearAndDisableTLSCachesOnCurrentThread() override { Base.ClearAndDisableTLSCachesOnCurrentThread(); }
	virtual void InitializeStatsMetadata() override { Base.InitializeStatsMetadata(); }
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return Base.Exec(InWorld, Cmd, Ar); }
	virtual void UpdateStats() override { Base.UpdateStats(); }
	virtual void GetAllocatorStats(FGenericMemoryStats& out_Stats) override { Base.GetAllocatorStats(out_Stats); }
	virtual void DumpAllocatorStats(FOutputDevice& Ar) override { Base.DumpAllocatorStats(Ar); }
	virtual bool IsInternallyThreadSafe() const override { return Base.IsInternallyThreadSafe(); }
	virtual bool ValidateHeap() override { return Base.ValidateHeap(); }
	virtual const TCHAR* GetDescriptiveName() override { return Base.GetDescriptiveName(); }
	virtual void OnMallocInitialized() override { Base.OnMallocInitialized(); }
	virtual void OnPreFork() override { Base.OnPreFork(); }
	virtual void OnPostFork() override { Base.OnPostFork(); }
};

VOXEL_RUN_ON_STARTUP_GAME()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("TestVoxelAlloc")))
	{
		return;
	}

	GMalloc = new FVoxelDebugMalloc(*GMalloc);
	GVoxelMallocDisableChecks = false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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
	VOXEL_ALLOW_MALLOC_SCOPE();
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
	VOXEL_ALLOW_MALLOC_SCOPE();
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
	VOXEL_ALLOW_MALLOC_SCOPE();
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