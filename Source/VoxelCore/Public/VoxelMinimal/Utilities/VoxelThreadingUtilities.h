// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Async/ParallelFor.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

template<ENamedThreads::Type Thread, ESubsequentsMode::Type SubsequentsMode = ESubsequentsMode::FireAndForget>
class TVoxelGraphTask
{
public:
	TVoxelUniqueFunction<void()> Lambda;

	explicit TVoxelGraphTask(TVoxelUniqueFunction<void()> Lambda)
		: Lambda(MoveTemp(Lambda))
	{
	}

	void DoTask(ENamedThreads::Type, const FGraphEventRef&) const
	{
		VOXEL_SCOPE_COUNTER("TVoxelGraphTask");
		Lambda();
	}

	static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(TVoxelGraphTask, STATGROUP_Voxel);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		return Thread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return SubsequentsMode;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_ENQUEUE_RENDER_COMMAND(Name) \
	INTELLISENSE_ONLY(struct VOXEL_APPEND_LINE(FDummy) { void Name(); }); \
	[](auto&& Lambda) \
	{ \
		ENQUEUE_RENDER_COMMAND(Name)([Lambda = MoveTemp(Lambda)](FRHICommandListImmediate& RHICmdList) \
		{ \
			VOXEL_SCOPE_COUNTER(#Name); \
			Lambda(RHICmdList); \
		}); \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE bool IsInGameThreadFast()
{
	const bool bIsInGameThread = FPlatformTLS::GetCurrentThreadId() == GGameThreadId;
	ensureVoxelSlowNoSideEffects(bIsInGameThread == IsInGameThread());
	return bIsInGameThread;
}

VOXELCORE_API void FlushVoxelGameThreadTasks();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API void AsyncBackgroundTaskImpl(TVoxelUniqueFunction<void()> Lambda);
VOXELCORE_API void AsyncThreadPoolTaskImpl(TVoxelUniqueFunction<void()> Lambda);

// Will never be called right away, even if we are on the game thread
// Useful to avoid deadlocks
VOXELCORE_API void RunOnGameThread_Async(TVoxelUniqueFunction<void()> Lambda);

template<typename LambdaType>
FORCEINLINE void RunOnGameThreadImpl(LambdaType Lambda)
{
	if (!IsInGameThreadFast())
	{
		RunOnGameThread_Async(MoveTemp(Lambda));
		return;
	}

	Lambda();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType, typename FutureType = TVoxelFutureType<typename TVoxelLambdaInfo<LambdaType>::ReturnType>>
FORCEINLINE FutureType AsyncBackgroundTask(LambdaType Lambda)
{
	const typename FutureType::PromiseType Promise;
	AsyncBackgroundTaskImpl([Lambda = MoveTemp(Lambda), Promise]
	{
		if constexpr (std::is_void_v<typename TVoxelLambdaInfo<LambdaType>::ReturnType>)
		{
			Lambda();
			Promise.Set();
		}
		else
		{
			Promise.Set(Lambda());
		}
	});
	return Promise.GetFuture();
}
template<typename LambdaType, typename FutureType = TVoxelFutureType<typename TVoxelLambdaInfo<LambdaType>::ReturnType>>
FORCEINLINE FutureType AsyncThreadPoolTask(LambdaType Lambda)
{
	const typename FutureType::PromiseType Promise;
	AsyncThreadPoolTaskImpl([Lambda = MoveTemp(Lambda), Promise]
	{
		if constexpr (std::is_void_v<typename TVoxelLambdaInfo<LambdaType>::ReturnType>)
		{
			Lambda();
			Promise.Set();
		}
		else
		{
			Promise.Set(Lambda());
		}
	});
	return Promise.GetFuture();
}
template<typename LambdaType, typename FutureType = TVoxelFutureType<typename TVoxelLambdaInfo<LambdaType>::ReturnType>>
FORCEINLINE FutureType RunOnGameThread(LambdaType Lambda)
{
	const typename FutureType::PromiseType Promise;
	RunOnGameThreadImpl([Lambda = MoveTemp(Lambda), Promise]
	{
		if constexpr (std::is_void_v<typename TVoxelLambdaInfo<LambdaType>::ReturnType>)
		{
			Lambda();
			Promise.Set();
		}
		else
		{
			Promise.Set(Lambda());
		}
	});
	return Promise.GetFuture();
}
template<typename LambdaType, typename FutureType = TVoxelFutureType<typename TVoxelLambdaInfo<LambdaType>::ReturnType>>
FORCEINLINE FutureType RunOnRenderThread(LambdaType Lambda)
{
	const typename FutureType::PromiseType Promise;
	VOXEL_ENQUEUE_RENDER_COMMAND(RunOnRenderThread)([Lambda = MoveTemp(Lambda), Promise](FRHICommandList& RHICmdList)
	{
		if constexpr (std::is_void_v<typename TVoxelLambdaInfo<LambdaType>::ReturnType>)
		{
			if constexpr (std::is_same_v<typename TVoxelLambdaInfo<LambdaType>::ArgTypes, TVoxelTypes<>>)
			{
				Lambda();
				Promise.Set();
			}
			else
			{
				Lambda(RHICmdList);
				Promise.Set();
			}
		}
		else
		{
			if constexpr (std::is_same_v<typename TVoxelLambdaInfo<LambdaType>::ArgTypes, TVoxelTypes<>>)
			{
				Promise.Set(Lambda());
			}
			else
			{
				Promise.Set(Lambda(RHICmdList));
			}
		}
	});
	return Promise.GetFuture();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
TSharedRef<T> MakeVoxelShareable_GameThread(T* Object)
{
	FVoxelMemory::CheckIsVoxelAlloc(Object);

	return TSharedPtr<T>(Object, [](T* InObject)
	{
		RunOnGameThread([=]
		{
			FVoxelMemory::Delete(InObject);
		});
	}).ToSharedRef();
}
template<typename T>
TSharedRef<T> MakeVoxelShareable_RenderThread(T* Object)
{
	FVoxelMemory::CheckIsVoxelAlloc(Object);

	return TSharedPtr<T>(Object, [](T* InObject)
	{
		VOXEL_ENQUEUE_RENDER_COMMAND(MakeVoxelShareable_RenderThread)([=](FRHICommandListImmediate& RHICmdList)
		{
			FVoxelMemory::Delete(InObject);
		});
	}).ToSharedRef();
}

template<typename T, typename... ArgTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
TSharedRef<T> MakeVoxelShared_GameThread(ArgTypes&&... Args)
{
	return MakeVoxelShareable_GameThread(new (GVoxelMemory) T(Forward<ArgTypes>(Args)...));
}
template<typename T, typename... ArgTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
TSharedRef<T> MakeVoxelShared_RenderThread(ArgTypes&&... Args)
{
	return MakeVoxelShareable_RenderThread(new (GVoxelMemory) T(Forward<ArgTypes>(Args)...));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct CParallelForLambdaHasIndex
{
	template<typename LambdaType, typename ValueType>
	auto Requires(LambdaType Lambda, ValueType Value, int32 Index) -> decltype(Lambda(Value, Index));
};

template<typename ArrayType, typename LambdaType>
std::enable_if_t<sizeof(GetNum(DeclVal<ArrayType>())) != 0> ParallelFor(
	ArrayType& Array,
	LambdaType Lambda,
	const EParallelForFlags Flags = EParallelForFlags::None,
	const int32 DefaultNumThreads = FTaskGraphInterface::Get().GetNumWorkerThreads())
{
	VOXEL_ALLOW_MALLOC_SCOPE();

	const int64 ArrayNum = GetNum(Array);
	const int64 NumThreads = FMath::Clamp<int64>(DefaultNumThreads, 1, ArrayNum);
	ParallelFor(NumThreads, [&](const int64 ThreadIndex)
	{
		const int64 ElementsPerThreads = FVoxelUtilities::DivideCeil_Positive(ArrayNum, NumThreads);

		const int64 StartIndex = ThreadIndex * ElementsPerThreads;
		const int64 EndIndex = FMath::Min((ThreadIndex + 1) * ElementsPerThreads, ArrayNum);

		for (int64 ElementIndex = StartIndex; ElementIndex < EndIndex; ElementIndex++)
		{
			if constexpr (TModels<CParallelForLambdaHasIndex, LambdaType, decltype(Array[ElementIndex])>::Value)
			{
				Lambda(Array[ElementIndex], ElementIndex);
			}
			else
			{
				Lambda(Array[ElementIndex]);
			}
		}
	}, Flags);
}

// Will starve the task graph
template<typename Type, typename SizeType, typename LambdaType>
void ParallelFor_ArrayView(
	const TVoxelArrayView<Type, SizeType> ArrayView,
	LambdaType Lambda,
	const int32 MaxNumThreads = FPlatformMisc::NumberOfCores())
{
	if (ArrayView.Num() == 0)
	{
		return;
	}

	const int64 NumThreads = FMath::Clamp<int64>(MaxNumThreads, 1, ArrayView.Num());
	ParallelFor(NumThreads, [&](const int64 ThreadIndex)
	{
		const int64 ElementsPerThreads = FVoxelUtilities::DivideCeil_Positive<int64>(ArrayView.Num(), NumThreads);

		const int64 StartIndex = ThreadIndex * ElementsPerThreads;
		const int64 EndIndex = FMath::Min<int64>((ThreadIndex + 1) * ElementsPerThreads, ArrayView.Num());

		if (StartIndex >= EndIndex)
		{
			// Will happen on small arrays
			return;
		}

		const TVoxelArrayView<Type, SizeType> ThreadArrayView = ArrayView.Slice(StartIndex, EndIndex - StartIndex);
		Lambda(ThreadArrayView);
	});
}