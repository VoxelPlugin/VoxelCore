// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/VoxelCriticalSection.h"
#include "VoxelMinimal/Containers/VoxelMap.h"

class FVoxelFuture;
class FVoxelPromise;

template<typename>
class TVoxelFuture;
template<typename>
class TVoxelPromise;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T, typename = void>
struct TVoxelFutureTypeImpl
{
	using Type = TVoxelFuture<T>;
};

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

template<typename Type>
using TVoxelPromiseType = typename TVoxelFutureType<Type>::PromiseType;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

enum class EVoxelFutureThread : uint8
{
	AnyThread,
	GameThread,
	RenderThread,
	AsyncThread,
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelPromiseState : public TSharedFromThis<FVoxelPromiseState>
{
public:
	~FVoxelPromiseState();
	UE_NONCOPYABLE(FVoxelPromiseState);

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

	void AddContinuation(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void(const FSharedVoidRef&)> Continuation);

private:
	using FThreadToContinuation = TVoxelInlineArray<TPair<EVoxelFutureThread, TVoxelUniqueFunction<void(const FSharedVoidRef&)>>, 1>;

	mutable FVoxelCriticalSection_NoPadding CriticalSection;
	TVoxelAtomic<bool> bIsComplete;
	FSharedVoidPtr Value_RequiresLock;
	FThreadToContinuation ThreadToContinuation_RequiresLock;
	TSharedPtr<FVoxelPromiseState> ThisPtr_RequiresLock;

	FVoxelPromiseState();

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

	static FVoxelFuture Done();

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
	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>>
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		const TVoxelPromiseType<ReturnType> Promise;
		PromiseState->AddContinuation(Thread, [Promise, Continuation = MoveTemp(Continuation)](const FSharedVoidRef&)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation();
				Promise.Set();
			}
			else
			{
				Promise.Set(Continuation());
			}
		});
		return Promise.GetFuture();
	}

#define Define(Thread) \
	template< \
		typename LambdaType, \
		typename ReturnType = LambdaReturnType_T<LambdaType>, \
		typename = LambdaHasSignature_T<LambdaType, ReturnType()>> \
	FORCEINLINE TVoxelFutureType<ReturnType> Then_ ## Thread(LambdaType Continuation) const \
	{ \
		return this->Then(EVoxelFutureThread::Thread, MoveTemp(Continuation)); \
	}

	Define(AnyThread);
	Define(GameThread);
	Define(RenderThread);
	Define(AsyncThread);

#undef Define

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

	template<typename OtherType, typename = std::enable_if_t<
		std::is_null_pointer_v<OtherType> &&
		// Wrap in a dummy type to disable this as copy constructor without failing to compile on clang if T is forward declared
		TIsConstructible<typename TChooseClass<std::is_same_v<OtherType, TVoxelFuture>, void, T>::Result, const OtherType&>::Value>>
	TVoxelFuture(const OtherType& OtherValue)
		: TVoxelFuture(T(OtherValue))
	{
	}

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
	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = LambdaHasSignature_T<LambdaType, ReturnType(const TSharedRef<T>&)>>
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		const TVoxelPromiseType<ReturnType> Promise;
		PromiseState->AddContinuation(Thread, [Promise, Continuation = MoveTemp(Continuation)](const FSharedVoidRef& Value)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation(ReinterpretCastRef<TSharedRef<T>>(Value));
				Promise.Set();
			}
			else
			{
				Promise.Set(Continuation(ReinterpretCastRef<TSharedRef<T>>(Value)));
			}
		});
		return Promise.GetFuture();
	}
	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, ReturnType(const T&)> ||
			(LambdaHasSignature_V<LambdaType, ReturnType(T)> && TIsTriviallyDestructible<LambdaDependentType_T<LambdaType, T>>::Value)>,
		typename = void>
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		const TVoxelPromiseType<ReturnType> Promise;
		PromiseState->AddContinuation(Thread, [Promise, Continuation = MoveTemp(Continuation)](const FSharedVoidRef& Value)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation(*ReinterpretCastRef<TSharedRef<T>>(Value));
				Promise.Set();
			}
			else
			{
				Promise.Set(Continuation(*ReinterpretCastRef<TSharedRef<T>>(Value)));
			}
		});
		return Promise.GetFuture();
	}

#define Define(Thread) \
	template< \
		typename LambdaType, \
		typename ReturnType = LambdaReturnType_T<LambdaType>, \
		typename = std::enable_if_t< \
			LambdaHasSignature_V<LambdaType, ReturnType(const TSharedRef<T>&)> || \
			LambdaHasSignature_V<LambdaType, ReturnType(const T&)> || \
			(LambdaHasSignature_V<LambdaType, ReturnType(T)> && TIsTriviallyDestructible<LambdaDependentType_T<LambdaType, T>>::Value)>, \
		typename = void> \
	FORCEINLINE TVoxelFutureType<ReturnType> Then_ ## Thread(LambdaType Continuation) const \
	{ \
		return this->Then(EVoxelFutureThread::Thread, MoveTemp(Continuation)); \
	}

	Define(AnyThread);
	Define(GameThread);
	Define(RenderThread);
	Define(AsyncThread);

#undef Define
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

#if INTELLISENSE_PARSER
	FORCEINLINE void Set(const decltype(nullptr)) const;
#endif

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

template<typename T>
FORCEINLINE TVoxelFuture<T>::TVoxelFuture(const TSharedRef<T>& Value)
{
	const TVoxelPromise<T> Promise;
	Promise.Set(Value);
	*this = Promise.GetFuture();
}