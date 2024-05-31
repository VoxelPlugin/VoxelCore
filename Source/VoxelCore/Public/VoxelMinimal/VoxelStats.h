// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "VoxelMacros.h"
#include "HAL/LowLevelMemStats.h"

UE_TRACE_CHANNEL_EXTERN(VoxelChannel, VOXELCORE_API);

DECLARE_STATS_GROUP(TEXT("Voxel"), STATGROUP_Voxel, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Voxel Counters"), STATGROUP_VoxelCounters, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Voxel Memory"), STATGROUP_VoxelMemory, STATCAT_Advanced);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
LLM_DECLARE_TAG_API(Voxel, VOXELCORE_API);
DECLARE_LLM_MEMORY_STAT_EXTERN(TEXT("Voxel"), STAT_VoxelLLM, STATGROUP_LLM, VOXELCORE_API);

extern VOXELCORE_API bool GVoxelLLMDisabled;

VOXELCORE_API void EnterVoxelLLMScope();
VOXELCORE_API void ExitVoxelLLMScope();
VOXELCORE_API void CheckVoxelLLMScope();

#define VOXEL_LLM_SCOPE() \
	if (!GVoxelLLMDisabled) \
	{ \
		EnterVoxelLLMScope(); \
	} \
	ON_SCOPE_EXIT_IMPL(VoxelLLM) \
	{ \
		if (!GVoxelLLMDisabled) \
		{ \
			ExitVoxelLLMScope(); \
		} \
	};
#else
#define VOXEL_LLM_SCOPE()
#endif

#if CPUPROFILERTRACE_ENABLED
FORCEINLINE bool AreVoxelStatsEnabled()
{
	return VoxelChannel.IsEnabled();
}

#define VOXEL_TRACE_ENABLED VOXEL_APPEND_LINE(__bTraceEnabled)

#define VOXEL_SCOPE_COUNTER_COND(Condition, Description) \
	VOXEL_LLM_SCOPE(); \
	const bool VOXEL_TRACE_ENABLED = AreVoxelStatsEnabled() && (Condition); \
	if (VOXEL_TRACE_ENABLED) \
	{ \
		VOXEL_ALLOW_MALLOC_SCOPE(); \
		static const FString StaticDescription = Description; \
		static const uint32 StaticSpecId = FCpuProfilerTrace::OutputEventType(*StaticDescription, __FILE__, __LINE__); \
		FCpuProfilerTrace::OutputBeginEvent(StaticSpecId); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_TRACE_ENABLED) \
		{ \
			VOXEL_ALLOW_MALLOC_SCOPE(); \
			FCpuProfilerTrace::OutputEndEvent(); \
		} \
	};

#define VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, Description) \
	VOXEL_LLM_SCOPE(); \
	const bool VOXEL_TRACE_ENABLED = AreVoxelStatsEnabled() && (Condition); \
	if (VOXEL_TRACE_ENABLED) \
	{ \
		VOXEL_ALLOW_MALLOC_SCOPE(); \
		ensureVoxelSlow(!Description.IsNone()); \
		FCpuProfilerTrace::OutputBeginDynamicEvent(Description, __FILE__, __LINE__); \
	} \
	ON_SCOPE_EXIT \
	{ \
		if (VOXEL_TRACE_ENABLED) \
		{ \
			VOXEL_ALLOW_MALLOC_SCOPE(); \
			FCpuProfilerTrace::OutputEndEvent(); \
		} \
	};

#else
FORCEINLINE bool AreVoxelStatsEnabled()
{
	return false;
}

#define VOXEL_SCOPE_COUNTER_COND(Condition, Description)
#define VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, Description)
#endif

VOXELCORE_API FString VoxelStats_CleanupFunctionName(const FString& FunctionName);
VOXELCORE_API FName VARARGS VoxelStats_PrintfImpl(const TCHAR* Format, ...);
VOXELCORE_API FName VoxelStats_AddNum(const FString& Format, int32 Num);

#if INTELLISENSE_PARSER
#define VoxelStats_PrintfImpl(...) FName(FString::Printf(__VA_ARGS__))
#endif

#define VOXEL_STATS_CLEAN_FUNCTION_NAME VoxelStats_CleanupFunctionName(__FUNCTION__)

#define VOXEL_SCOPE_COUNTER_FORMAT_COND(Condition, Format, ...) VOXEL_SCOPE_COUNTER_FNAME_COND(Condition, VoxelStats_PrintfImpl(TEXT(Format), ##__VA_ARGS__))
#define VOXEL_FUNCTION_COUNTER_COND(Condition) VOXEL_SCOPE_COUNTER_COND(Condition, VOXEL_STATS_CLEAN_FUNCTION_NAME)
#define VOXEL_INLINE_COUNTER_COND(Condition, Name, ...) ([&]() -> decltype(auto) { VOXEL_SCOPE_COUNTER_COND(Condition, VOXEL_STATS_CLEAN_FUNCTION_NAME + TEXT(".") + FString(Name)); return __VA_ARGS__; }())

#define VOXEL_SCOPE_COUNTER(Description) VOXEL_SCOPE_COUNTER_COND(true, Description)
#define VOXEL_SCOPE_COUNTER_FNAME(Description) VOXEL_SCOPE_COUNTER_FNAME_COND(true, Description)
#define VOXEL_SCOPE_COUNTER_FORMAT(Format, ...) VOXEL_SCOPE_COUNTER_FORMAT_COND(true, Format, ##__VA_ARGS__)
#define VOXEL_FUNCTION_COUNTER() VOXEL_FUNCTION_COUNTER_COND(true)
#define VOXEL_INLINE_COUNTER(Name, ...) VOXEL_INLINE_COUNTER_COND(true, Name, ##__VA_ARGS__)

#define VOXEL_SCOPE_COUNTER_NUM(Name, Num, Threshold) VOXEL_SCOPE_COUNTER_FNAME_COND((Num) > (Threshold), VoxelStats_AddNum(STATIC_FSTRING(Name), Num))
#define VOXEL_FUNCTION_COUNTER_NUM(Num, Threshold) VOXEL_SCOPE_COUNTER_NUM(VOXEL_STATS_CLEAN_FUNCTION_NAME, Num, Threshold)

#define VOXEL_LOG_FUNCTION_STATS() FScopeLogTime PREPROCESSOR_JOIN(FScopeLogTime_, __LINE__)(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME));
#define VOXEL_LOG_SCOPE_STATS(Name) FScopeLogTime PREPROCESSOR_JOIN(FScopeLogTime_, __LINE__)(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME + "." + FString(Name)));
#define VOXEL_TRACE_BOOKMARK() TRACE_BOOKMARK(*STATIC_FSTRING(VOXEL_STATS_CLEAN_FUNCTION_NAME));

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE_VOXEL_COUNTER(API, StatName, Name) DECLARE_VOXEL_COUNTER_WITH_CATEGORY(API, STATGROUP_VoxelCounters, StatName, Name)

#define DECLARE_VOXEL_COUNTER_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FVoxelCounter64 StatName;) \
	DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)


#define DECLARE_VOXEL_FRAME_COUNTER(API, StatName, Name) DECLARE_VOXEL_FRAME_COUNTER_WITH_CATEGORY(API, STATGROUP_VoxelCounters, StatName, Name)

#define DECLARE_VOXEL_FRAME_COUNTER_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FVoxelCounter64 StatName;) \
	DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)


#define DEFINE_VOXEL_COUNTER(StatName) \
	VOXEL_DEBUG_ONLY(FVoxelCounter64 StatName;) \
	DEFINE_STAT(StatName ## _Stat)

#if STATS
#define INC_VOXEL_COUNTER_BY(StatName, Amount) \
	VOXEL_DEBUG_ONLY(StatName.Add(Amount);) \
	VOXEL_ALLOW_MALLOC_INLINE(FThreadStats::AddMessage(GET_STATFNAME(StatName ## _Stat), EStatOperation::Add, int64(Amount)));

#define DEC_VOXEL_COUNTER_BY(StatName, Amount) \
	VOXEL_DEBUG_ONLY(StatName.Subtract(Amount);) \
	VOXEL_ALLOW_MALLOC_INLINE(FThreadStats::AddMessage(GET_STATFNAME(StatName ## _Stat), EStatOperation::Subtract, int64(Amount)));
#else
#define INC_VOXEL_COUNTER_BY(StatName, Amount)
#define DEC_VOXEL_COUNTER_BY(StatName, Amount)
#endif

#define DEC_VOXEL_COUNTER(StatName) DEC_VOXEL_COUNTER_BY(StatName, 1)
#define INC_VOXEL_COUNTER(StatName) INC_VOXEL_COUNTER_BY(StatName, 1)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE_VOXEL_MEMORY_STAT(API, StatName, Name) DECLARE_VOXEL_MEMORY_STAT_WITH_CATEGORY(API, STATGROUP_VoxelMemory, StatName, Name)

#define DECLARE_VOXEL_MEMORY_STAT_WITH_CATEGORY(API, Category, StatName, Name) \
	VOXEL_DEBUG_ONLY(extern API FVoxelCounter64 StatName;) \
	DECLARE_MEMORY_STAT_EXTERN(TEXT(Name), StatName ## _Stat, Category, API)

#define DEFINE_VOXEL_MEMORY_STAT(StatName) \
	VOXEL_DEBUG_ONLY(FVoxelCounter64 StatName;) \
	DEFINE_STAT(StatName ## _Stat)

#define INC_VOXEL_MEMORY_STAT_BY(StatName, Amount) INC_VOXEL_COUNTER_BY(StatName, Amount)
#define DEC_VOXEL_MEMORY_STAT_BY(StatName, Amount) DEC_VOXEL_COUNTER_BY(StatName, Amount)

#if STATS
#define GET_VOXEL_MEMORY_STAT(Name) (INTELLISENSE_ONLY((void)&Name,) StatPtr_ ## Name ## _Stat)
#else
#define GET_VOXEL_MEMORY_STAT(Name)
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define FVoxelStatsRefHelper VOXEL_APPEND_LINE(__FVoxelStatsRefHelper)

#if STATS
#define VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate) \
	struct FVoxelStatsRefHelper \
	{ \
		FVoxelCounter64 AllocatedSize; \
		\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize.Set(Other.AllocatedSize.Exchange_ReturnOld(0)); \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize.Set(Other.AllocatedSize.Exchange_ReturnOld(0)); \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize.Set(Other.AllocatedSize.Get()); \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize.Get()); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			const int64 NewAllocatedSize = Other.AllocatedSize.Get(); \
			const int64 OldAllocatedSize = AllocatedSize.Exchange_ReturnOld(NewAllocatedSize); \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, OldAllocatedSize); \
			INC_VOXEL_MEMORY_STAT_BY(StatName, NewAllocatedSize); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize.Get()); \
		} \
	}; \
	mutable FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper); \
	void UpdateStats() const \
	{ \
		const int64 NewAllocatedSize = GetAllocatedSize(); \
		const int64 OldAllocatedSize = VOXEL_APPEND_LINE(__VoxelStatsRefHelper).AllocatedSize.Exchange_ReturnOld(NewAllocatedSize); \
		\
		if (NewAllocatedSize == OldAllocatedSize) \
		{ \
			return; \
		} \
		\
		DEC_VOXEL_MEMORY_STAT_BY(StatName, OldAllocatedSize); \
		INC_VOXEL_MEMORY_STAT_BY(StatName, NewAllocatedSize); \
	} \
	void EnsureStatsAreUpToDate() const \
	{ \
		ensure(VOXEL_APPEND_LINE(__VoxelStatsRefHelper).AllocatedSize.Get() == GetAllocatedSize()); \
	}

#define VOXEL_TYPE_SIZE_TRACKER(Type, StatName) \
	struct FVoxelStatsRefHelper \
	{ \
		FVoxelStatsRefHelper() \
		{ \
			INC_VOXEL_MEMORY_STAT_BY(StatName, sizeof(Type)); \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, sizeof(Type)); \
		} \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&&) \
		{ \
			return *this; \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper&) \
		{ \
			return *this; \
		} \
	}; \
	FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper);

#define VOXEL_ALLOCATED_SIZE_TRACKER_CUSTOM(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize.Set(Other.AllocatedSize.Get()); \
			Other.AllocatedSize.Set(0); \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			AllocatedSize.Set(Other.AllocatedSize.Get()); \
			Other.AllocatedSize.Set(0); \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize.Get()); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			AllocatedSize = Other.AllocatedSize; \
			INC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize.Get()); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, AllocatedSize.Get()); \
		} \
		FVoxelStatsRefHelper& operator=(const int64 NewAllocatedSize) \
		{ \
			const int64 OldAllocatedSize = AllocatedSize.Exchange_ReturnOld(NewAllocatedSize); \
			DEC_VOXEL_MEMORY_STAT_BY(StatName, OldAllocatedSize); \
			INC_VOXEL_MEMORY_STAT_BY(StatName, NewAllocatedSize); \
			return *this; \
		} \
	private: \
		FVoxelCounter64 AllocatedSize; \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_COUNTER_HELPER(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper(FVoxelStatsRefHelper&& Other) \
		{ \
			Value = Other.Value; \
			Other.Value = 0; \
		} \
		FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&& Other) \
		{ \
			Value = Other.Value; \
			Other.Value = 0; \
			return *this; \
		} \
		FVoxelStatsRefHelper(const FVoxelStatsRefHelper& Other) \
		{ \
			Value = Other.Value; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
		} \
		FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper& Other) \
		{ \
			Value = Other.Value; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
			return *this; \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			DEC_VOXEL_COUNTER_BY(StatName, Value); \
		} \
		FVoxelStatsRefHelper& operator=(int64 NewValue) \
		{ \
			DEC_VOXEL_COUNTER_BY(StatName, Value); \
			Value = NewValue; \
			INC_VOXEL_COUNTER_BY(StatName, Value); \
			return *this; \
		} \
	private: \
		int64 Value = 0; \
	}; \
	mutable FVoxelStatsRefHelper Name;

VOXELCORE_API void Voxel_AddAmountToDynamicStat(FName Name, int64 Amount);

#else
#define VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate) \
	void UpdateStats() const \
	{ \
	} \
	void EnsureStatsAreUpToDate() const \
	{ \
	}

#define VOXEL_ALLOCATED_SIZE_TRACKER_CUSTOM(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		void UpdateStats() \
		{ \
		} \
		FVoxelStatsRefHelper& operator+=(int64 InAllocatedSize) \
		{ \
			return *this; \
		} \
		FVoxelStatsRefHelper& operator=(int64 InAllocatedSize) \
		{ \
			return *this; \
		} \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_COUNTER_HELPER(StatName, Name) \
	class FVoxelStatsRefHelper \
	{ \
	public:\
		FVoxelStatsRefHelper() = default; \
		FVoxelStatsRefHelper& operator=(int64 NewValue) \
		{ \
			return *this; \
		} \
	}; \
	mutable FVoxelStatsRefHelper Name;

#define VOXEL_TYPE_SIZE_TRACKER(Type, StatName)

FORCEINLINE void Voxel_AddAmountToDynamicStat(FName Name, int64 Amount)
{
}

#endif

#define VOXEL_ALLOCATED_SIZE_TRACKER(StatName) \
	VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetAllocatedSize, UpdateStats, EnsureStatsAreUpToDate)

#define VOXEL_ALLOCATED_SIZE_GPU_TRACKER(StatName) \
	VOXEL_ALLOCATED_SIZE_TRACKER_IMPL(StatName, GetGpuAllocatedSize, UpdateGpuStats, EnsureGpuStatsAreUpToDate)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelStackTrace
{
public:
	FVoxelStackTrace() = default;

	void Capture();

private:
	TArray<void*, TFixedAllocator<128>> StackFrames;
};

#define VOXEL_TRACK_INSTANCES_SLOW() \
	struct FVoxelInstanceTrackerSlow \
	{ \
	public: \
		FVoxelInstanceTrackerSlow() \
		{ \
			Initialize(); \
		} \
		FVoxelInstanceTrackerSlow(FVoxelInstanceTrackerSlow&&) \
		{ \
			Initialize(); \
		} \
		FVoxelInstanceTrackerSlow(const FVoxelInstanceTrackerSlow&) \
		{ \
			Initialize(); \
		} \
		FVoxelInstanceTrackerSlow& operator=(const FVoxelInstanceTrackerSlow&) \
		{ \
			return *this; \
		} \
		FVoxelInstanceTrackerSlow& operator=(FVoxelInstanceTrackerSlow&&) \
		{ \
			return *this; \
		} \
		~FVoxelInstanceTrackerSlow(); \
	\
	private: \
		void Initialize(); \
		\
		int32 InstanceIndex = -1; \
		FVoxelStackTrace StackTrace; \
	}; \
	FVoxelInstanceTrackerSlow _InstanceTrackerSlow; \

#define DEFINE_VOXEL_INSTANCE_TRACKER_SLOW(Struct) \
	struct FVoxelInstanceTrackerSlowData ## Struct \
	{ \
		FVoxelCriticalSection CriticalSection; \
		TVoxelSparseArray<Struct*> InstanceIndexToThis; \
	}; \
	FVoxelInstanceTrackerSlowData ## Struct GVoxelInstanceTrackerData ## Struct; \
	\
	void Struct::FVoxelInstanceTrackerSlow::Initialize() \
	{ \
		check(InstanceIndex == -1); \
		Struct* This = reinterpret_cast<Struct*>(reinterpret_cast<uint8*>(this) - offsetof(Struct, _InstanceTrackerSlow)); \
		VOXEL_SCOPE_LOCK(GVoxelInstanceTrackerData ## Struct.CriticalSection); \
		InstanceIndex = GVoxelInstanceTrackerData ## Struct.InstanceIndexToThis.Add(This); \
		\
		StackTrace.Capture(); \
	} \
	Struct::FVoxelInstanceTrackerSlow::~FVoxelInstanceTrackerSlow() \
	{ \
		VOXEL_SCOPE_LOCK(GVoxelInstanceTrackerData ## Struct.CriticalSection); \
		GVoxelInstanceTrackerData ## Struct.InstanceIndexToThis.RemoveAt(InstanceIndex); \
	} \
	\
	class FVoxelTicker_TrackInstancesSlow_ ## Struct : public FVoxelTicker { virtual void Tick() override {} }; \
	VOXEL_RUN_ON_STARTUP_GAME() \
	{ \
		new FVoxelTicker_TrackInstancesSlow_ ## Struct(); \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
#define VOXEL_COUNT_INSTANCES() \
	static FVoxelCounter64 VoxelInstanceCount; \
	struct FVoxelStatsRefHelper \
	{ \
		FORCEINLINE FVoxelStatsRefHelper() \
		{ \
			ensureVoxelSlow(VoxelInstanceCount.Increment_ReturnNew() > 0); \
		} \
		FORCEINLINE ~FVoxelStatsRefHelper() \
		{ \
			ensureVoxelSlow(VoxelInstanceCount.Decrement_ReturnNew() >= 0); \
		} \
		FORCEINLINE FVoxelStatsRefHelper(FVoxelStatsRefHelper&&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FORCEINLINE FVoxelStatsRefHelper(const FVoxelStatsRefHelper&) \
			: FVoxelStatsRefHelper() \
		{ \
		} \
		FORCEINLINE FVoxelStatsRefHelper& operator=(FVoxelStatsRefHelper&&) \
		{ \
			return *this; \
		} \
		FORCEINLINE FVoxelStatsRefHelper& operator=(const FVoxelStatsRefHelper&) \
		{ \
			return *this; \
		} \
	}; \
	FVoxelStatsRefHelper VOXEL_APPEND_LINE(__VoxelStatsRefHelper);

#define DEFINE_VOXEL_INSTANCE_COUNTER(Struct) \
	DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num " #Struct), STAT_Num ## Struct, STATGROUP_VoxelCounters); \
	FVoxelCounter64 Struct::VoxelInstanceCount; \
	\
	VOXEL_RUN_ON_STARTUP_GAME() \
	{ \
		VOXELCORE_API void RegisterVoxelInstanceCounter(FName StatName, const FVoxelCounter64& Counter); \
		RegisterVoxelInstanceCounter(GET_STATFNAME(STAT_Num ## Struct), Struct::VoxelInstanceCount); \
	}
#else
#define VOXEL_COUNT_INSTANCES()
#define DEFINE_VOXEL_INSTANCE_COUNTER(Struct)
#endif