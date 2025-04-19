// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

struct VOXELCORE_API FVoxelShouldCancel
{
public:
	FVoxelShouldCancel();

	FORCEINLINE operator bool() const
	{
		return ShouldCancelTasks.Get(std::memory_order_relaxed);
	}

private:
	const TVoxelAtomic<bool>& ShouldCancelTasks;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel
{
	extern VOXELCORE_API TMulticastDelegate<void(bool& bAnyTaskProcessed)> OnFlushGameTasks;
	VOXELCORE_API void FlushGameTasks();

	extern VOXELCORE_API FSimpleMulticastDelegate OnForceTick;
	VOXELCORE_API void ForceTick();

	VOXELCORE_API bool ShouldCancel();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FVoxelFuture ExecuteSynchronously_Impl(TFunctionRef<FVoxelFuture()> Lambda);

	template<typename LambdaType, typename Type = typename LambdaReturnType_T<LambdaType>::Type>
	requires LambdaHasSignature_V<LambdaType, TVoxelFuture<Type>()>
	FORCEINLINE TSharedRef<Type> ExecuteSynchronously(LambdaType Lambda)
	{
		const FVoxelFuture Future = Voxel::ExecuteSynchronously_Impl(Lambda);
		return static_cast<const TVoxelFuture<Type>&>(Future).GetSharedValueChecked();
	}

	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, FVoxelFuture()>
	FORCEINLINE void ExecuteSynchronously(LambdaType Lambda)
	{
		Voxel::ExecuteSynchronously_Impl(Lambda);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
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
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> GameTask_Async(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::GameThread, MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> RenderTask(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::RenderThread, MoveTemp(Lambda));
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandList&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandListBase&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(FRHICommandListImmediate&)>
	)
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

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> AsyncTask(LambdaType Lambda)
	{
		return FVoxelFuture::Execute(EVoxelFutureThread::AsyncThread, MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API void AsyncTask_ThreadPool_Impl(TVoxelUniqueFunction<void()> Lambda);

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> AsyncTask_ThreadPool(LambdaType Lambda)
	{
		TVoxelPromiseType<ReturnType> Promise;
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
		return Promise;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

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

	template<typename T, typename... ArgTypes>
	requires std::is_constructible_v<T, ArgTypes...>
	TSharedRef<T> MakeShared_GameThread(ArgTypes&&... Args)
	{
		return MakeShareable_GameThread(new T(Forward<ArgTypes>(Args)...));
	}
	template<typename T, typename... ArgTypes>
	requires std::is_constructible_v<T, ArgTypes...>
	TSharedRef<T> MakeShared_RenderThread(ArgTypes&&... Args)
	{
		return MakeShareable_RenderThread(new T(Forward<ArgTypes>(Args)...));
	}
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
	void FlushTasks();

private:
	TQueue<UE::Tasks::TTask<void>, EQueueMode::Mpsc> Tasks;
};