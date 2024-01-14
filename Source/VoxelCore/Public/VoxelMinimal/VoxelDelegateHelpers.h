// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"

template<typename...>
struct TVoxelTypes;

template<typename>
struct TVoxelFunctionInfo;

template<typename InReturnType, typename... InArgTypes>
struct TVoxelFunctionInfo<InReturnType(InArgTypes...)>
{
	using ReturnType = InReturnType;
	using ArgTypes = TVoxelTypes<InArgTypes...>;
	using FuncType = InReturnType(InArgTypes...);
};

template<typename ReturnType, typename Class, typename... ArgTypes>
struct TVoxelFunctionInfo<ReturnType(Class::*)(ArgTypes...) const> : TVoxelFunctionInfo<ReturnType(ArgTypes...)>
{
};

template<typename ReturnType, typename Class, typename... ArgTypes>
struct TVoxelFunctionInfo<ReturnType(Class::*)(ArgTypes...)> : TVoxelFunctionInfo<ReturnType(ArgTypes...)>
{
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType, typename = void>
struct TVoxelLambdaInfo : TVoxelFunctionInfo<void()>
{
	static_assert(sizeof(LambdaType) == -1, "Generic lambdas (eg [](auto)) are not supported");
};

template<typename LambdaType>
struct TVoxelLambdaInfo<LambdaType, typename TEnableIf<sizeof(decltype(&LambdaType::operator())) != 0>::Type> : TVoxelFunctionInfo<decltype(&LambdaType::operator())>
{
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_ENGINE_VERSION >= 503
struct FVoxelDelegateUtilities
	: public TDelegateBase<FThreadSafeDelegateMode>
	, public TDelegateBase<FNotThreadSafeDelegateMode>
	, public TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>
{
	template<typename Mode>
	struct THack : TDelegateBase<Mode>
	{
		using TDelegateBase<Mode>::CreateDelegateInstance;
	};

	template<typename Mode, typename DelegateInstanceType>
	static void CreateDelegateInstance(TDelegateBase<Mode>& Base, DelegateInstanceType& DelegateInstance)
	{
		static_cast<THack<Mode>&>(Base).template CreateDelegateInstance<DelegateInstanceType>(DelegateInstance);
	}
	template<typename DelegateInstanceType, typename Mode, typename... DelegateInstanceParams>
	static void CreateDelegateInstance(TDelegateBase<Mode>& Base, DelegateInstanceParams&&... Params)
	{
		static_cast<THack<Mode>&>(Base).template CreateDelegateInstance<DelegateInstanceType>(Forward<DelegateInstanceParams>(Params)...);
	}
};
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename, typename>
class TSharedPtrLambdaDelegateInstance;

template<typename UserPolicy, typename ReturnType, typename... ArgTypes>
class TSharedPtrLambdaDelegateInstance<UserPolicy, ReturnType, TVoxelTypes<ArgTypes...>> final : public IBaseDelegateInstance<ReturnType(ArgTypes...), UserPolicy>
{
public:
	const FDelegateHandle Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	const FWeakVoidPtr WeakPtr;
	const TFunction<ReturnType(ArgTypes...)> Lambda;

	TSharedPtrLambdaDelegateInstance(
		const FWeakVoidPtr& WeakPtr,
		TFunction<ReturnType(ArgTypes...)>&& Lambda)
		: WeakPtr(WeakPtr)
		, Lambda(MoveTemp(Lambda))
	{
	}

	template<typename DelegateBase>
	FORCEINLINE static void Create(DelegateBase& Base, const FWeakVoidPtr& WeakPtr, TFunction<ReturnType(ArgTypes...)>&& Lambda)
	{
#if VOXEL_ENGINE_VERSION >= 503
		FVoxelDelegateUtilities::CreateDelegateInstance<TSharedPtrLambdaDelegateInstance>(Base, WeakPtr, MoveTemp(Lambda));
#else
		new (Base) TSharedPtrLambdaDelegateInstance(WeakPtr, MoveTemp(Lambda));
#endif
	}

public:
	//~ Begin IDelegateInstance Interface
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
	virtual FName TryGetBoundFunctionName() const override
	{
		return {};
	}
#endif
	virtual UObject* GetUObject() const override
	{
		return nullptr;
	}
	const void* GetObjectForTimerManager() const override
	{
		return nullptr;
	}
	virtual uint64 GetBoundProgramCounterForTimerManager() const override
	{
		return 0;
	}
	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return WeakPtr.HasSameObject(InUserObject);
	}
	virtual bool IsSafeToExecute() const
	{
		return WeakPtr.IsValid();
	}
	virtual FDelegateHandle GetHandle() const
	{
		return Handle;
	}
	//~ End IDelegateInstance Interface

public:
	//~ Begin IBaseDelegateInstance Interface
#if VOXEL_ENGINE_VERSION >= 503
	virtual void CreateCopy(TDelegateBase<FThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
#else
	virtual void CreateCopy(FDelegateBase& Base) const override
	{
		new (Base) TSharedPtrLambdaDelegateInstance(*this);
	}
#endif

	virtual ReturnType Execute(ArgTypes... Args) const override
	{
		const FSharedVoidPtr SharedPtr = WeakPtr.Pin();
		check(SharedPtr);
		return Lambda(Forward<ArgTypes>(Args)...);
	}
	virtual bool ExecuteIfSafe(ArgTypes... Args) const override
	{
		const FSharedVoidPtr SharedPtr = WeakPtr.Pin();
		if (!SharedPtr)
		{
			return false;
		}
		Lambda(Forward<ArgTypes>(Args)...);
		return true;
	}
	//~ End IBaseDelegateInstance Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType>
FORCEINLINE auto MakeLambdaDelegate(LambdaType Lambda)
{
	return TDelegate<typename TVoxelLambdaInfo<LambdaType>::FuncType>::CreateLambda(MoveTemp(Lambda));
}

template<typename UserPolicy = FDefaultDelegateUserPolicy, typename T, typename LambdaType>
FORCEINLINE auto MakeWeakPtrDelegate(const T& Ptr, LambdaType Lambda)
{
	using Info = TVoxelLambdaInfo<LambdaType>;
	TDelegate<typename Info::FuncType, UserPolicy> Delegate;
	TSharedPtrLambdaDelegateInstance<
		UserPolicy,
		typename Info::ReturnType,
		typename Info::ArgTypes>::Create(Delegate, MakeWeakVoidPtr(MakeWeakPtr(Ptr)), MoveTemp(Lambda));
	return Delegate;
}

template<typename T, typename LambdaType>
FORCEINLINE auto MakeTSWeakPtrDelegate(const T& Ptr, LambdaType Lambda)
{
	return MakeWeakPtrDelegate<FDefaultTSDelegateUserPolicy>(Ptr, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename>
struct TMakeWeakPtrLambdaHelper;

template<typename... ArgTypes>
struct TMakeWeakPtrLambdaHelper<TVoxelTypes<ArgTypes...>>
{
	template<typename T, typename LambdaType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda)
	{
		return [WeakPtr = MakeWeakPtr(Ptr), Lambda = MoveTemp(Lambda)](ArgTypes... Args)
		{
			if (const auto Pinned = WeakPtr.Pin())
			{
				Lambda(Forward<ArgTypes>(Args)...);
			}
		};
	}
	template<typename T, typename LambdaType, typename ReturnType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda, ReturnType&& Default)
	{
		return [WeakPtr = MakeWeakPtr(Ptr), Lambda = MoveTemp(Lambda), Default = MoveTemp(Default)](ArgTypes... Args) -> ReturnType
		{
			if (const auto Pinned = WeakPtr.Pin())
			{
				return Lambda(Forward<ArgTypes>(Args)...);
			}
			else
			{
				return Default;
			}
		};
	}
};

template<
	typename T,
	typename LambdaType,
	typename Info = TVoxelLambdaInfo<LambdaType>,
	typename = typename TEnableIf<std::is_same_v<typename Info::ReturnType, void>>::Type>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeWeakPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda));
}

template<
	typename T,
	typename LambdaType,
	typename Info = TVoxelLambdaInfo<LambdaType>,
	typename = typename TEnableIf<!std::is_same_v<typename Info::ReturnType, void>>::Type,
	typename = typename TEnableIf<FVoxelUtilities::CanMakeSafe<typename Info::ReturnType>>::Type>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeWeakPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda), FVoxelUtilities::MakeSafe<typename Info::ReturnType>());
}

template<
	typename T,
	typename LambdaType,
	typename Info = TVoxelLambdaInfo<LambdaType>,
	typename = typename TEnableIf<!std::is_same_v<typename Info::ReturnType, void>>::Type>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda, typename Info::ReturnType&& Default)
{
	return TMakeWeakPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda), MoveTemp(Default));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename>
struct TMakeWeakObjectPtrLambdaHelper;

template<typename... ArgTypes>
struct TMakeWeakObjectPtrLambdaHelper<TVoxelTypes<ArgTypes...>>
{
	template<typename T, typename LambdaType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda)
	{
		return [WeakPtr = MakeWeakObjectPtr(Cast<UObject>(Ptr)), Lambda = MoveTemp(Lambda)](ArgTypes... Args)
		{
			checkVoxelSlow(IsInGameThread());

			if (WeakPtr.IsValid())
			{
				Lambda(Forward<ArgTypes>(Args)...);
			}
		};
	}
	template<typename T, typename LambdaType, typename ReturnType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda, ReturnType&& Default)
	{
		return [WeakPtr = MakeWeakObjectPtr(Cast<UObject>(Ptr)), Lambda = MoveTemp(Lambda), Default = MoveTemp(Default)](ArgTypes... Args) -> ReturnType
		{
			checkVoxelSlow(IsInGameThread());

			if (WeakPtr.IsValid())
			{
				return Lambda(Forward<ArgTypes>(Args)...);
			}
			else
			{
				return Default;
			}
		};
	}
};

template<typename T, typename LambdaType, typename Info = TVoxelLambdaInfo<LambdaType>, typename = typename TEnableIf<std::is_same_v<typename Info::ReturnType, void>>::Type>
FORCEINLINE auto MakeWeakObjectPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeWeakObjectPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda));
}

template<typename T, typename LambdaType, typename Info = TVoxelLambdaInfo<LambdaType>, typename = typename TEnableIf<!std::is_same_v<typename Info::ReturnType, void>>::Type>
FORCEINLINE auto MakeWeakObjectPtrLambda(const T& Ptr, LambdaType Lambda, typename Info::ReturnType&& Default = {})
{
	return TMakeWeakObjectPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda), MoveTemp(Default));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename>
struct TMakeStrongPtrLambdaHelper;

template<typename... ArgTypes>
struct TMakeStrongPtrLambdaHelper<TVoxelTypes<ArgTypes...>>
{
	template<typename T, typename LambdaType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda)
	{
		return [StrongPtr = MakeSharedRef(Ptr), Lambda = MoveTemp(Lambda)](ArgTypes... Args)
		{
			(void)StrongPtr;
			return Lambda(Forward<ArgTypes>(Args)...);
		};
	}
};

template<typename T, typename LambdaType, typename Info = TVoxelLambdaInfo<LambdaType>>
FORCEINLINE auto MakeStrongPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeStrongPtrLambdaHelper<typename Info::ArgTypes>::Make(Ptr, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename, typename>
class TForwardDelegateInstance;

template<typename WeakDelegateBase, typename ReturnType, typename... ArgTypes>
class TForwardDelegateInstance<WeakDelegateBase, ReturnType, TVoxelTypes<ArgTypes...>> final : public IBaseDelegateInstance<ReturnType(ArgTypes...), FDefaultDelegateUserPolicy>
{
public:
	const FDelegateHandle Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	const TSharedRef<WeakDelegateBase> WeakDelegate;
	const TFunction<ReturnType(ArgTypes...)> Lambda;

	explicit TForwardDelegateInstance(
		const TSharedRef<WeakDelegateBase>& WeakDelegate,
		TFunction<ReturnType(ArgTypes...)>&& Lambda)
		: WeakDelegate(WeakDelegate)
		, Lambda(MoveTemp(Lambda))
	{
	}

	template<typename DelegateBase>
	FORCEINLINE static void Create(
		DelegateBase& Base,
		const TSharedRef<WeakDelegateBase>& WeakDelegate,
		TFunction<ReturnType(ArgTypes...)>&& Lambda)
	{
#if VOXEL_ENGINE_VERSION >= 503
		FVoxelDelegateUtilities::CreateDelegateInstance<TForwardDelegateInstance>(Base, WeakDelegate, MoveTemp(Lambda));
#else
		new (Base) TForwardDelegateInstance(WeakDelegate, MoveTemp(Lambda));
#endif
	}

public:
	//~ Begin IDelegateInstance Interface
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
	virtual FName TryGetBoundFunctionName() const override
	{
		return {};
	}
#endif
	virtual UObject* GetUObject() const override
	{
		return nullptr;
	}
	const void* GetObjectForTimerManager() const override
	{
		return nullptr;
	}
	virtual uint64 GetBoundProgramCounterForTimerManager() const override
	{
		return 0;
	}
	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return false;
	}
	virtual bool IsSafeToExecute() const
	{
		return WeakDelegate->IsBound();
	}
	virtual FDelegateHandle GetHandle() const
	{
		return Handle;
	}
	//~ End IDelegateInstance Interface

public:
	//~ Begin IBaseDelegateInstance Interface
#if VOXEL_ENGINE_VERSION >= 503
	virtual void CreateCopy(TDelegateBase<FThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
#else
	virtual void CreateCopy(FDelegateBase& Base) const override
	{
		new (Base) TForwardDelegateInstance(*this);
	}
#endif

	virtual ReturnType Execute(ArgTypes... Args) const override
	{
		ensure(IsSafeToExecute());
		return Lambda(Forward<ArgTypes>(Args)...);
	}
	virtual bool ExecuteIfSafe(ArgTypes... Args) const override
	{
		if (!WeakDelegate->IsBound())
		{
			return false;
		}

		Lambda(Forward<ArgTypes>(Args)...);
		return true;
	}
	//~ End IBaseDelegateInstance Interface
};

// Makes a new delegate with the same lifetime as WeakDelegate
template<typename DelegateType, typename LambdaType>
FORCEINLINE auto MakeWeakDelegateDelegate(const DelegateType& WeakDelegate, LambdaType Lambda)
{
	using Info = TVoxelLambdaInfo<LambdaType>;
	TDelegate<typename Info::FuncType> Delegate;
	TForwardDelegateInstance<DelegateType, typename Info::ReturnType, typename Info::ArgTypes>::Create(Delegate, MakeSharedCopy(WeakDelegate), MoveTemp(Lambda));
	return Delegate;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename... ArgTypes>
class TMulticastForwardDelegateInstance final : public IBaseDelegateInstance<void(ArgTypes...), FDefaultDelegateUserPolicy>
{
public:
	const FDelegateHandle Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	const TMulticastDelegate<void(ArgTypes...)> Delegate;

	explicit TMulticastForwardDelegateInstance(const TMulticastDelegate<void(ArgTypes...)>& Delegate)
		: Delegate(Delegate)
	{
	}

	template<typename DelegateBase>
	FORCEINLINE static void Create(DelegateBase& Base, const TMulticastDelegate<void(ArgTypes...)>& Delegate)
	{
#if VOXEL_ENGINE_VERSION >= 503
		FVoxelDelegateUtilities::CreateDelegateInstance<TMulticastForwardDelegateInstance>(Base, Delegate);
#else
		new (Base) TMulticastForwardDelegateInstance(Base, Delegate);
#endif
	}

public:
	//~ Begin IDelegateInstance Interface
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
	virtual FName TryGetBoundFunctionName() const override
	{
		return {};
	}
#endif
	virtual UObject* GetUObject() const override
	{
		return nullptr;
	}
	const void* GetObjectForTimerManager() const override
	{
		return nullptr;
	}
	virtual uint64 GetBoundProgramCounterForTimerManager() const override
	{
		return 0;
	}
	virtual bool HasSameObject(const void* InUserObject) const override
	{
		return false;
	}
	virtual bool IsSafeToExecute() const
	{
		return Delegate.IsBound();
	}
	virtual FDelegateHandle GetHandle() const
	{
		return Handle;
	}
	//~ End IDelegateInstance Interface

public:
	//~ Begin IBaseDelegateInstance Interface
#if VOXEL_ENGINE_VERSION >= 503
	virtual void CreateCopy(TDelegateBase<FThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
	virtual void CreateCopy(TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>& Base) const override
	{
		FVoxelDelegateUtilities::CreateDelegateInstance(Base, *this);
	}
#else
	virtual void CreateCopy(FDelegateBase& Base) const override
	{
		new (Base) TMulticastForwardDelegateInstance(*this);
	}
#endif

	virtual void Execute(ArgTypes... Args) const override
	{
		ensure(IsSafeToExecute());
		Delegate.Broadcast(Forward<ArgTypes>(Args)...);
	}
	virtual bool ExecuteIfSafe(ArgTypes... Args) const override
	{
		if (!Delegate.IsBound())
		{
			return false;
		}

		Delegate.Broadcast(Forward<ArgTypes>(Args)...);
		return true;
	}
	//~ End IBaseDelegateInstance Interface
};

template<typename... ArgTypes>
FORCEINLINE TDelegate<void(ArgTypes...)> MakeMulticastForward(const TMulticastDelegate<void(ArgTypes...)>& Delegate)
{
	TDelegate<void(ArgTypes...)> NewDelegate;
	TMulticastForwardDelegateInstance<ArgTypes...>::Create(NewDelegate, Delegate);
	return NewDelegate;
}