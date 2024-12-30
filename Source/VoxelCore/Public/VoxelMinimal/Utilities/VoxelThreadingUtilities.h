// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Async/ParallelFor.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Containers/VoxelMap.h"
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

namespace Voxel
{
	extern VOXELCORE_API TMulticastDelegate<void(bool& bAnyTaskProcessed)> OnFlushGameTasks;
	VOXELCORE_API void FlushGameTasks();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> GameTask(LambdaType Lambda)
	{
		if (IsInGameThread())
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Lambda();
				return {};
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

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API void AsyncTask_ThreadPool_Impl(TVoxelUniqueFunction<void()> Lambda);

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> AsyncTask_ThreadPool(LambdaType Lambda)
	{
		const TVoxelPromiseType<ReturnType> Promise;
		AsyncTask_ThreadPool_Impl([Lambda = MoveTemp(Lambda), Promise]
		{
			if constexpr (std::is_void_v<ReturnType>)
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
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
TSharedRef<T> MakeShareable_GameThread(T* Object)
{
	return TSharedPtr<T>(Object, [](T* InObject)
	{
		Voxel::GameTask([=]
		{
			delete InObject;
		});
	}).ToSharedRef();
}
template<typename T>
TSharedRef<T> MakeShareable_RenderThread(T* Object)
{
	return TSharedPtr<T>(Object, [](T* InObject)
	{
		Voxel::RenderTask([=]
		{
			delete InObject;
		});
	}).ToSharedRef();
}

template<typename T, typename... ArgTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
TSharedRef<T> MakeShared_GameThread(ArgTypes&&... Args)
{
	return MakeShareable_GameThread(new T(Forward<ArgTypes>(Args)...));
}
template<typename T, typename... ArgTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgTypes...>>>
TSharedRef<T> MakeShared_RenderThread(ArgTypes&&... Args)
{
	return MakeShareable_RenderThread(new T(Forward<ArgTypes>(Args)...));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<
	typename Type,
	typename SizeType,
	typename LambdaType,
	typename = std::enable_if_t<
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)>>>
void ParallelFor(
	const TVoxelArrayView<Type, SizeType> ArrayView,
	LambdaType Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	if (ArrayView.Num() == 0)
	{
		return;
	}

	const SizeType NumThreads = FMath::Clamp<SizeType>(FPlatformMisc::NumberOfCoresIncludingHyperthreads(), 1, ArrayView.Num());
	ParallelFor(NumThreads, [&](const int32 ThreadIndex)
	{
		const SizeType ElementsPerThreads = FVoxelUtilities::DivideCeil_Positive<SizeType>(ArrayView.Num(), NumThreads);

		const SizeType StartIndex = ThreadIndex * ElementsPerThreads;
		const SizeType EndIndex = FMath::Min<SizeType>((ThreadIndex + 1) * ElementsPerThreads, ArrayView.Num());

		if (StartIndex >= EndIndex)
		{
			// Will happen on small arrays
			return;
		}

		if constexpr (
			LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
			LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)>)
		{
			Lambda(ArrayView.Slice(StartIndex, EndIndex - StartIndex));
		}
		else if constexpr (
			LambdaHasSignature_V<LambdaType, void(Type&)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&)>)
		{
			for (SizeType Index = StartIndex; Index < EndIndex; Index++)
			{
				Lambda(ArrayView[Index]);
			}
		}
		else if constexpr (
			LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)>)
		{
			for (SizeType Index = StartIndex; Index < EndIndex; Index++)
			{
				Lambda(ArrayView[Index], Index);
			}
		}
		else
		{
			checkStatic(std::is_same_v<LambdaType, void>);
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<
	typename Type,
	typename Allocator,
	typename LambdaType,
	typename SizeType = typename Allocator::SizeType,
	typename = std::enable_if_t<
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)>>>
void ParallelFor(
	TArray<Type, Allocator>& Array,
	LambdaType Lambda)
{
	ParallelFor(
		MakeVoxelArrayView(Array),
		MoveTemp(Lambda));
}

template<
	typename Type,
	typename Allocator,
	typename LambdaType,
	typename SizeType = typename Allocator::SizeType,
	typename = std::enable_if_t<
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)>>>
void ParallelFor(
	const TArray<Type, Allocator>& Array,
	LambdaType Lambda)
{
	ParallelFor(
		MakeVoxelArrayView(Array),
		MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<
	typename KeyType,
	typename ValueType,
	typename ArrayType,
	typename LambdaType,
	typename = LambdaHasSignature_T<LambdaType, void(typename TVoxelMap<KeyType, ValueType, ArrayType>::FElement&)>>
void ParallelFor(
	TVoxelMap<KeyType, ValueType, ArrayType>& Map,
	LambdaType Lambda)
{
	return ParallelFor(
		Map.GetElements(),
		MoveTemp(Lambda));
}

template<
	typename KeyType,
	typename ValueType,
	typename ArrayType,
	typename LambdaType,
	typename = LambdaHasSignature_T<LambdaType, void(const typename TVoxelMap<KeyType, ValueType, ArrayType>::FElement&)>>
void ParallelFor(
	const TVoxelMap<KeyType, ValueType, ArrayType>& Map,
	LambdaType Lambda)
{
	return ParallelFor(
		Map.GetElements(),
		MoveTemp(Lambda));
}
template<
	typename KeyType,
	typename ValueType,
	typename ArrayType,
	typename LambdaType,
	typename = LambdaHasSignature_T<LambdaType, void(const KeyType&)>>
void ParallelFor_Keys(
	const TVoxelMap<KeyType, ValueType, ArrayType>& Map,
	LambdaType Lambda)
{
	return ParallelFor(
		Map.GetElements(),
		[&](const typename TVoxelMap<KeyType, ValueType, ArrayType>::FElement& Element)
		{
			Lambda(Element.Key);
		});
}

template<
	typename KeyType,
	typename ValueType,
	typename ArrayType,
	typename LambdaType,
	typename = LambdaHasSignature_T<LambdaType, void(ValueType&)>>
void ParallelFor_Values(
	TVoxelMap<KeyType, ValueType, ArrayType>& Map,
	LambdaType Lambda)
{
	return ParallelFor(
		Map.GetElements(),
		[&](typename TVoxelMap<KeyType, ValueType, ArrayType>::FElement& Element)
		{
			Lambda(Element.Value);
		});
}

template<
	typename KeyType,
	typename ValueType,
	typename ArrayType,
	typename LambdaType,
	typename = LambdaHasSignature_T<LambdaType, void(const ValueType&)>>
void ParallelFor_Values(
	const TVoxelMap<KeyType, ValueType, ArrayType>& Map,
	LambdaType Lambda)
{
	return ParallelFor(
		Map.GetElements(),
		[&](const typename TVoxelMap<KeyType, ValueType, ArrayType>::FElement& Element)
		{
			Lambda(Element.Value);
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelParallelTaskScope
{
public:
	FVoxelParallelTaskScope() = default;
	~FVoxelParallelTaskScope();

	void AddTask(TVoxelUniqueFunction<void()> Lambda);

private:
	TVoxelChunkedArray<UE::Tasks::TTask<void>> Tasks;
};