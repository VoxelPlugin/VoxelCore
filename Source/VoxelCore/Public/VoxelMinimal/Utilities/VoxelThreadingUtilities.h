// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Async/ParallelFor.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

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

namespace Voxel
{
	VOXELCORE_API void GameTask_SkipDispatcher(TVoxelUniqueFunction<void()> Lambda);
	VOXELCORE_API void RenderTask_SkipDispatcher(TVoxelUniqueFunction<void()> Lambda);
	VOXELCORE_API void AsyncTask_SkipDispatcher(TVoxelUniqueFunction<void()> Lambda);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> GameTask(LambdaType Lambda)
	{
		if (IsInGameThreadFast())
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Lambda();
				return FVoxelFuture::Done();
			}
			else
			{
				return Lambda();
			}
		}

		return FVoxelFuture::Execute(EVoxelFutureThread::GameThread, MoveTemp(Lambda));
	}

	// Will never be called right away, even if we are on the game thread
	// Useful to avoid deadlocks
	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> GameTask_Async(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::GameThread, MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> RenderTask(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::RenderThread, MoveTemp(Lambda));
	}

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandList&)> ||
			LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandListBase&)> ||
			LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandListImmediate&)>>,
		typename = void>
	FORCEINLINE TVoxelFutureType<ReturnType> RenderTask(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::RenderThread, [Lambda = MoveTemp(Lambda)]
		{
			Lambda(FRHICommandListImmediate::Get());
		});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> AsyncTask(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::AsyncThread, MoveTemp(Lambda));
	}
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
		Voxel::GameTask([=]
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
	return MakeVoxelShareable_GameThread(new(GVoxelMemory) T(Forward<ArgTypes>(Args)...));
}
template<typename T, typename... ArgTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
TSharedRef<T> MakeVoxelShared_RenderThread(ArgTypes&&... Args)
{
	return MakeVoxelShareable_RenderThread(new(GVoxelMemory) T(Forward<ArgTypes>(Args)...));
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace FVoxelUtilities
{
	VOXELCORE_API void Task(TVoxelUniqueFunction<void()> Lambda);
	VOXELCORE_API void FlushTasks();
}

class VOXELCORE_API FVoxelTaskScope
{
public:
	explicit FVoxelTaskScope(bool bAllowParallelTasks);
	~FVoxelTaskScope();

private:
	const bool bPreviousAllowParallelTasks;
};