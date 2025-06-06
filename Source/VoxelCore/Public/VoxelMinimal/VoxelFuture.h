// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/VoxelRefCountPtr.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"

struct FVoxelFuture;
class FVoxelPromise;
class FVoxelPromiseState;
class FVoxelTaskContext;

template<typename>
class TVoxelFuture;
template<typename>
class TVoxelPromise;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelFutureTypeImpl
{
	using Type = TVoxelFuture<T>;
};

template<typename T>
struct TVoxelFutureTypeImpl<TSharedRef<T>>
{
	using Type = TVoxelFuture<T>;
};

template<>
struct TVoxelFutureTypeImpl<FVoxelFuture>
{
	using Type = FVoxelFuture;
};

template<typename T>
struct TVoxelFutureTypeImpl<TVoxelFuture<T>>
{
	using Type = TVoxelFuture<T>;
};

template<>
struct TVoxelFutureTypeImpl<FVoxelPromise>
{
	using Type = FVoxelFuture;
};

template<typename T>
struct TVoxelFutureTypeImpl<TVoxelPromise<T>>
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

class VOXELCORE_API IVoxelPromiseState
{
public:
	static TVoxelRefCountPtr<IVoxelPromiseState> New(
		FVoxelTaskContext* ContextOverride,
		bool bWithValue);

	static TVoxelRefCountPtr<IVoxelPromiseState> New(const FSharedVoidRef& Value);

	UE_NONCOPYABLE(IVoxelPromiseState);

public:
	FORCEINLINE bool IsComplete() const
	{
		return bIsComplete.Get();
	}
	FORCEINLINE const FSharedVoidRef& GetSharedValueChecked() const
	{
		// We don't lock - if bIsComplete is true, Value is set
		checkVoxelSlow(bHasValue);
		checkVoxelSlow(bIsComplete.Get());
		checkVoxelSlow(Value.IsValid());
		return ToSharedRefFast(Value);
	}

public:
	FORCEINLINE void AddRef()
	{
		NumRefs.Increment();
	}
	FORCEINLINE void Release()
	{
		if (NumRefs.Decrement_ReturnNew() == 0)
		{
			Destroy();
		}
	}
	FORCEINLINE int32 GetRefCount() const
	{
		return NumRefs.Get();
	}

public:
	void Set();
	void Set(const FSharedVoidRef& NewValue);

public:
#if VOXEL_DEBUG
	void CheckCanAddContinuation(const FVoxelFuture& Future);
#else
	void CheckCanAddContinuation(const FVoxelFuture& Future) {}
#endif

	void AddContinuation(const FVoxelFuture& Future);

	void AddContinuation(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Continuation);

	void AddContinuation(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void(const FSharedVoidRef&)> Continuation);

protected:
	VOXEL_COUNT_INSTANCES();

	const bool bHasValue;
	TVoxelAtomic<bool> bIsComplete;
	TVoxelAtomic<bool> bIsLocked;
	int32 StackIndex = -1;
	int32 KeepAliveIndex = -1;
	FVoxelCounter32 NumRefs;
	FSharedVoidPtr Value;

	FORCEINLINE explicit IVoxelPromiseState(const bool bHasValue)
		: bHasValue(bHasValue)
	{
	}

	void Destroy();

	friend FVoxelPromiseState;
};
checkStatic(sizeof(IVoxelPromiseState) == 32);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FVoxelFuture() is a valid, completed future
struct VOXELCORE_API FVoxelFuture
{
public:
	using PromiseType = FVoxelPromise;

	FVoxelFuture() = default;
	explicit FVoxelFuture(TConstVoxelArrayView<FVoxelFuture> Futures);

	template<typename... FutureTypes>
	requires
	(
		sizeof...(FutureTypes) != 1 &&
		(std::derived_from<std::remove_reference_t<FutureTypes>, FVoxelFuture> && ...)
	)
	FVoxelFuture(FutureTypes&&... Futures)
	{
		checkStatic(sizeof...(Futures) > 1);

		PromiseState = IVoxelPromiseState::New(nullptr, false);

		FVoxelCounter32* Counter = new FVoxelCounter32(sizeof...(Futures));

		const auto AddFuture = [&](const FVoxelFuture& Future)
		{
			if (Future.IsComplete())
			{
				if (Counter->Decrement_ReturnNew() == 0)
				{
					PromiseState->Set();
					delete Counter;
				}
				return;
			}

			Future.PromiseState->CheckCanAddContinuation(*this);
			Future.PromiseState->AddContinuation(EVoxelFutureThread::AnyThread, [Counter, LocalPromiseState = PromiseState]
			{
				if (Counter->Decrement_ReturnNew() == 0)
				{
					LocalPromiseState->Set();
					delete Counter;
				}
			});
		};
		VOXEL_FOLD_EXPRESSION(AddFuture(Futures));
	}

public:
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	static FORCEINLINE TVoxelFutureType<ReturnType> Execute(
		const EVoxelFutureThread Thread,
		LambdaType Lambda)
	{
		TVoxelPromiseType<ReturnType> Promise;
		FVoxelFuture::ExecuteImpl(Thread, [Lambda = MoveTemp(Lambda), Promise]
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

private:
	static void ExecuteImpl(
		EVoxelFutureThread Thread,
		TVoxelUniqueFunction<void()> Lambda);

public:
	FORCEINLINE bool IsComplete() const
	{
		return
			!PromiseState ||
			PromiseState->IsComplete();
	}

public:
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		if (IsComplete())
		{
			return Execute(Thread, MoveTemp(Continuation));
		}

		TVoxelPromiseType<ReturnType> Promise;
		PromiseState->AddContinuation(Thread, [Promise, Continuation = MoveTemp(Continuation)]
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
		return Promise;
	}

#define Define(Thread, Suffix) \
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>> \
	requires LambdaHasSignature_V<LambdaType, ReturnType()> \
	FORCEINLINE TVoxelFutureType<ReturnType> Then_ ## Thread ## Suffix(LambdaType Continuation) const \
	{ \
		return this->Then(EVoxelFutureThread::Thread, MoveTemp(Continuation)); \
	}

	Define(AnyThread,);
	Define(GameThread, _Async);
	Define(RenderThread,);
	Define(AsyncThread,);

#undef Define

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires LambdaHasSignature_V<LambdaType, ReturnType()>
	FORCEINLINE TVoxelFutureType<ReturnType> Then_GameThread(LambdaType Continuation) const
	{
		if (IsComplete() &&
			IsInGameThread())
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation();
				return {};
			}
			else
			{
				return Continuation();
			}
		}

		return this->Then(EVoxelFutureThread::GameThread, MoveTemp(Continuation));
	}

protected:
	TVoxelRefCountPtr<IVoxelPromiseState> PromiseState;

	FORCEINLINE explicit FVoxelFuture(TVoxelRefCountPtr<IVoxelPromiseState>&& PromiseState)
		: PromiseState(MoveTemp(PromiseState))
	{
	}

	template<typename>
	friend class TVoxelFuture;
	template<typename>
	friend class TVoxelPromise;

	friend FVoxelPromise;
	friend FVoxelPromiseState;
	friend IVoxelPromiseState;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
class TVoxelFuture : public FVoxelFuture
{
public:
	using Type = T;
	using PromiseType = TVoxelPromise<T>;

	// Futures are always valid
	TVoxelFuture()
		: TVoxelFuture(MakeShared<T>(FVoxelUtilities::MakeSafe<T>()))
	{
		checkStatic(FVoxelUtilities::CanMakeSafe<T>);
	}

	template<typename ChildType>
	requires
	(
		std::is_same_v<ChildType, T> ||
		std::derived_from<ChildType, T>
	)
	FORCEINLINE TVoxelFuture(const TSharedRef<ChildType>& Value)
		: FVoxelFuture(IVoxelPromiseState::New(MakeSharedVoidRef(Value)))
	{
	}

	// nullptr constructor, a bit convoluted to fix some compile errors
	template<typename NullType>
	requires
	(
		std::is_null_pointer_v<NullType> &&
		// Wrap in a dummy type to disable this as copy constructor without failing to compile on clang if T is forward declared
		std::is_constructible_v<std::conditional_t<std::is_same_v<NullType, TVoxelFuture>, void, T>, const NullType&>
	)
	TVoxelFuture(const NullType& OtherValue)
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

	FORCEINLINE const TSharedRef<T>& GetSharedValueChecked() const
	{
		return ReinterpretCastRef<TSharedRef<T>>(PromiseState->GetSharedValueChecked());
	}
	FORCEINLINE T& GetValueChecked() const
	{
		return *GetSharedValueChecked();
	}

	FORCEINLINE operator const TVoxelFuture<const T>&() const
	{
		return ReinterpretCastRef<TVoxelFuture<const T>>(*this);
	}

public:
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		LambdaHasSignature_V<LambdaType, ReturnType(TSharedRef<T>)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(const TSharedRef<T>&)>
	)
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		TVoxelPromiseType<ReturnType> Promise;
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
		return Promise;
	}
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		LambdaHasSignature_V<LambdaType, ReturnType(const T&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(T&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(T)>
	)
	FORCEINLINE TVoxelFutureType<ReturnType> Then(
		const EVoxelFutureThread Thread,
		LambdaType Continuation) const
	{
		TVoxelPromiseType<ReturnType> Promise;
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
		return Promise;
	}

#define Define(Thread, Suffix) \
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>> \
	requires \
	( \
		LambdaHasSignature_V<LambdaType, ReturnType(TSharedRef<T>)> || \
		LambdaHasSignature_V<LambdaType, ReturnType(const TSharedRef<T>&)> || \
		LambdaHasSignature_V<LambdaType, ReturnType(const T&)> || \
		LambdaHasSignature_V<LambdaType, ReturnType(T&)> || \
		LambdaHasSignature_V<LambdaType, ReturnType(T)> \
	) \
	FORCEINLINE TVoxelFutureType<ReturnType> Then_ ## Thread ## Suffix(LambdaType Continuation) const \
	{ \
		return this->Then(EVoxelFutureThread::Thread, MoveTemp(Continuation)); \
	}

	Define(AnyThread,);
	Define(GameThread, _Async);
	Define(RenderThread,);
	Define(AsyncThread,);

#undef Define

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		LambdaHasSignature_V<LambdaType, ReturnType(TSharedRef<T>)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(const TSharedRef<T>&)>
	)
	FORCEINLINE TVoxelFutureType<ReturnType> Then_GameThread(LambdaType Continuation) const
	{
		if (IsComplete() &&
			IsInGameThread())
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation(ReinterpretCastRef<TSharedRef<T>>(GetSharedValueChecked()));
				return {};
			}
			else
			{
				return Continuation(ReinterpretCastRef<TSharedRef<T>>(GetSharedValueChecked()));
			}
		}

		return this->Then(EVoxelFutureThread::GameThread, MoveTemp(Continuation));
	}
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		LambdaHasSignature_V<LambdaType, ReturnType(const T&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(T&)> ||
		LambdaHasSignature_V<LambdaType, ReturnType(T)>
	)
	FORCEINLINE TVoxelFutureType<ReturnType> Then_GameThread(LambdaType Continuation) const
	{
		if (IsComplete() &&
			IsInGameThread())
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				Continuation(*ReinterpretCastRef<TSharedRef<T>>(GetSharedValueChecked()));
				return {};
			}
			else
			{
				return Continuation(*ReinterpretCastRef<TSharedRef<T>>(GetSharedValueChecked()));
			}
		}

		return this->Then(EVoxelFutureThread::GameThread, MoveTemp(Continuation));
	}

protected:
	FORCEINLINE explicit TVoxelFuture(TVoxelRefCountPtr<IVoxelPromiseState>&& PromiseState)
		: FVoxelFuture(MoveTemp(PromiseState))
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelPromise : public FVoxelFuture
{
public:
	FORCEINLINE FVoxelPromise(FVoxelTaskContext* ContextOverride = nullptr)
		: FVoxelFuture(IVoxelPromiseState::New(ContextOverride, false))
	{
	}

	FORCEINLINE void Set() const
	{
		PromiseState->Set();
	}
	FORCEINLINE void Set(const FVoxelFuture& Future) const
	{
		if (Future.IsComplete())
		{
			PromiseState->Set();
		}
		else
		{
			Future.PromiseState->AddContinuation(*this);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
class TVoxelPromise : public TVoxelFuture<T>
{
public:
	FORCEINLINE TVoxelPromise(FVoxelTaskContext* ContextOverride = nullptr)
		: TVoxelFuture<T>(IVoxelPromiseState::New(ContextOverride, true))
	{
	}

	template<typename NullType>
	requires
	(
		std::is_same_v<decltype(nullptr), NullType> &&
		TIsTSharedPtr_V<T>
	)
	FORCEINLINE void Set(const NullType& Value) const
	{
		this->Set(T(Value));
	}

	FORCEINLINE void Set(const T& Value) const
	{
		this->Set(MakeSharedCopy(Value));
	}
	FORCEINLINE void Set(T&& Value) const
	{
		this->Set(MakeSharedCopy(MoveTemp(Value)));
	}
	FORCEINLINE void Set(const TSharedRef<T>& Value) const
	{
		this->PromiseState->Set(MakeSharedVoidRef(Value));
	}
	FORCEINLINE void Set(const TVoxelFuture<T>& Future) const
	{
		if (Future.IsComplete())
		{
			this->Set(Future.GetSharedValueChecked());
		}
		else
		{
			Future.PromiseState->AddContinuation(*this);
		}
	}
};