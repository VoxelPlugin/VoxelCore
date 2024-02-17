// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/VoxelCriticalSection.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Containers/VoxelMap.h"

class FVoxelFuture;
class FVoxelPromise;

template<typename>
class TVoxelFuture;
template<typename>
class TVoxelPromise;

enum class EVoxelFutureThread : uint8
{
	AnyThread,
	GameThread,
	RenderThread,
	BackgroundThread,
	VoxelThread,
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelPromiseState : public TSharedFromThis<FVoxelPromiseState>
{
public:
	UE_NONCOPYABLE(FVoxelPromiseState);
	~FVoxelPromiseState();

	VOXEL_COUNT_INSTANCES();

	FORCEINLINE bool IsComplete() const
	{
		return bIsComplete.Get();
	}
	FORCEINLINE FSharedVoidRef GetValueChecked() const
	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		checkVoxelSlow(bIsComplete.Get());
		return Value_RequiresLock.ToSharedRef();
	}

public:
	void Set(const FSharedVoidRef& Value);
	void Set(const FVoxelFuture& Future);

	void AddOnComplete(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void(const FSharedVoidRef&)> OnComplete);

private:
	using FThreadToOnComplete = TVoxelInlineArray<TPair<EVoxelFutureThread, TVoxelUniqueFunction<void(const FSharedVoidRef&)>>, 1>;

	mutable FVoxelCriticalSection_NoPadding CriticalSection;
	TVoxelAtomic<bool> bIsComplete;
	FSharedVoidPtr Value_RequiresLock;
	FThreadToOnComplete ThreadToOnComplete_RequiresLock;
	TSharedPtr<FVoxelPromiseState> ThisPtr_RequiresLock;

	FVoxelPromiseState() = default;

	template<typename LambdaType>
	static void Dispatch(
		EVoxelFutureThread Thread,
		LambdaType Lambda);

	friend FVoxelPromise;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelFuture
{
public:
	using PromiseType = FVoxelPromise;

	FVoxelFuture() = default;
	explicit FVoxelFuture(TConstVoxelArrayView<FVoxelFuture> Futures);

public:
	FORCEINLINE bool IsValid() const
	{
		return PromiseState.IsValid();
	}
	FORCEINLINE bool IsComplete() const
	{
		return PromiseState->IsComplete();
	}

public:
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()())>>>
	FORCEINLINE void OnComplete_AnyThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::AnyThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef&)
		{
			OnComplete();
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()())>>>
	FORCEINLINE void OnComplete_GameThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::GameThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef&)
		{
			OnComplete();
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()())>>>
	FORCEINLINE void OnComplete_RenderThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::RenderThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef&)
		{
			OnComplete();
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()())>>>
	FORCEINLINE void OnComplete_BackgroundThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::BackgroundThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef&)
		{
			OnComplete();
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()())>>>
	FORCEINLINE void OnComplete_VoxelThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::VoxelThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef&)
		{
			OnComplete();
		});
	}

protected:
	TSharedPtr<FVoxelPromiseState> PromiseState;

	FORCEINLINE explicit FVoxelFuture(const TSharedRef<FVoxelPromiseState>& PromiseState)
		: PromiseState(PromiseState)
	{
	}

	friend FVoxelPromise;
	friend FVoxelPromiseState;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelPromise
{
public:
	FVoxelPromise() = default;

	FORCEINLINE FVoxelFuture GetFuture() const
	{
		return FVoxelFuture(PromiseState);
	}

	void Set() const;

protected:
	TSharedRef<FVoxelPromiseState> PromiseState = MakeVoxelShareable(new (GVoxelMemory) FVoxelPromiseState());
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
class TVoxelFuture : public FVoxelFuture
{
public:
	using PromiseType = TVoxelPromise<T>;

	TVoxelFuture() = default;
	TVoxelFuture(const TSharedRef<T>& Value);

	FORCEINLINE TVoxelFuture(const T& Value)
		: TVoxelFuture(MakeSharedCopy(Value))
	{
	}
	FORCEINLINE TVoxelFuture(T&& Value)
		: TVoxelFuture(MakeSharedCopy(MoveTemp(Value)))
	{
	}

	FORCEINLINE TSharedRef<T> GetValueChecked() const
	{
		return ReinterpretCastRef<TSharedRef<T>>(PromiseState->GetValueChecked());
	}

public:
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<const TSharedRef<T>&>()))>>, typename = void>
	FORCEINLINE void OnComplete_AnyThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::AnyThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<const TSharedRef<T>&>()))>>, typename = void>
	FORCEINLINE void OnComplete_GameThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::GameThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<const TSharedRef<T>&>()))>>, typename = void>
	FORCEINLINE void OnComplete_RenderThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::RenderThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<const TSharedRef<T>&>()))>>, typename = void>
	FORCEINLINE void OnComplete_BackgroundThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::BackgroundThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<const TSharedRef<T>&>()))>>, typename = void>
	FORCEINLINE void OnComplete_VoxelThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::VoxelThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}

public:
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
	FORCEINLINE void OnComplete_AnyThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::AnyThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(*ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
	FORCEINLINE void OnComplete_GameThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::GameThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(*ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
	FORCEINLINE void OnComplete_RenderThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::RenderThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(*ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
	FORCEINLINE void OnComplete_BackgroundThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::BackgroundThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(*ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
	template<typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
	FORCEINLINE void OnComplete_VoxelThread(LambdaType OnComplete) const
	{
		PromiseState->AddOnComplete(EVoxelFutureThread::VoxelThread, [OnComplete = MoveTemp(OnComplete)](const FSharedVoidRef& Value)
		{
			OnComplete(*ReinterpretCastRef<TSharedRef<T>>(Value));
		});
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
class TVoxelPromise : private FVoxelPromise
{
public:
	TVoxelPromise() = default;

	FORCEINLINE TVoxelFuture<T> GetFuture() const
	{
		return ReinterpretCastRef<TVoxelFuture<T>>(FVoxelPromise::GetFuture());
	}

	FORCEINLINE void Set(const T& Value) const
	{
		PromiseState->Set(MakeSharedVoidRef(MakeSharedCopy(Value)));
	}
	FORCEINLINE void Set(T&& Value) const
	{
		PromiseState->Set(MakeSharedVoidRef(MakeSharedCopy(MoveTemp(Value))));
	}
	FORCEINLINE void Set(const TSharedRef<T>& Value) const
	{
		PromiseState->Set(MakeSharedVoidRef(Value));
	}
	FORCEINLINE void Set(const TVoxelFuture<T>& Future) const
	{
		PromiseState->Set(Future);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename = void>
struct TVoxelFutureTypeImpl;

template<typename T>
struct TVoxelFutureTypeImpl<TSharedRef<T>>
{
	using Type = TVoxelFuture<T>;
};

template<typename T>
struct TVoxelFutureTypeImpl<TVoxelFuture<T>>
{
	using Type = TVoxelFuture<T>;
};

template<>
struct TVoxelFutureTypeImpl<void>
{
	using Type = FVoxelFuture;
};

template<typename Type>
using TVoxelFutureType = typename TVoxelFutureTypeImpl<Type>::Type;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API TVoxelFuture<bool> operator&&(const TVoxelFuture<bool>& A, const TVoxelFuture<bool>& B);

template<typename T>
FORCEINLINE TVoxelFuture<T>::TVoxelFuture(const TSharedRef<T>& Value)
{
	const TVoxelPromise<T> Promise;
	Promise.Set(Value);
	*this = Promise.GetFuture();
}