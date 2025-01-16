// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

struct FVoxelDelegateUtilities
	: public TDelegateBase<FThreadSafeDelegateMode>
	, public TDelegateBase<FNotThreadSafeDelegateMode>
	, public TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>
{
#if VOXEL_ENGINE_VERSION >= 505
	template<typename Mode, typename DelegateInstanceType>
	static void CreateDelegateInstance(TDelegateBase<Mode>& Base, DelegateInstanceType& DelegateInstance)
	{
		new (TWriteLockedDelegateAllocation{Base}) DelegateInstanceType(DelegateInstance);
	}
	template<typename DelegateInstanceType, typename Mode, typename... DelegateInstanceParams>
	static void CreateDelegateInstance(TDelegateBase<Mode>& Base, DelegateInstanceParams&&... Params)
	{
		new (TWriteLockedDelegateAllocation{Base}) DelegateInstanceType(Forward<DelegateInstanceParams>(Params)...);
	}
#else
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
#endif
};

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
		FVoxelDelegateUtilities::CreateDelegateInstance<TSharedPtrLambdaDelegateInstance>(Base, WeakPtr, MoveTemp(Lambda));
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
	return TDelegate<LambdaSignature_T<LambdaType>>::CreateLambda(MoveTemp(Lambda));
}

template<typename UserPolicy = FDefaultDelegateUserPolicy, typename T, typename LambdaType>
FORCEINLINE auto MakeWeakPtrDelegate(const T& Ptr, LambdaType Lambda)
{
#if VOXEL_DEBUG
	CheckLambdaDoesNotCaptureSharedPtr(MakeWeakPtr(Ptr).Pin(), Lambda);
#endif

	TDelegate<LambdaSignature_T<LambdaType>, UserPolicy> Delegate;

	TSharedPtrLambdaDelegateInstance<UserPolicy, LambdaReturnType_T<LambdaType>, LambdaArgTypes_T<LambdaType>>::Create(
		Delegate,
		MakeWeakVoidPtr(MakeWeakPtr(Ptr)),
		MoveTemp(Lambda));

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
	typename = std::enable_if_t<std::is_void_v<LambdaReturnType_T<LambdaType>>>>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
#if VOXEL_DEBUG
	CheckLambdaDoesNotCaptureSharedPtr(MakeWeakPtr(Ptr).Pin(), Lambda);
#endif

	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(Ptr, MoveTemp(Lambda));
}

template<
	typename T,
	typename LambdaType,
	typename ReturnType = LambdaReturnType_T<LambdaType>,
	typename = std::enable_if_t<!std::is_void_v<ReturnType> && FVoxelUtilities::CanMakeSafe<ReturnType>>>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
#if VOXEL_DEBUG
	CheckLambdaDoesNotCaptureSharedPtr(MakeWeakPtr(Ptr).Pin(), Lambda);
#endif

	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(
		Ptr,
		MoveTemp(Lambda),
		FVoxelUtilities::MakeSafe<ReturnType>());
}

template<
	typename T,
	typename LambdaType,
	typename ReturnType = LambdaReturnType_T<LambdaType>,
	typename = std::enable_if_t<!std::is_void_v<ReturnType>>>
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda, ReturnType&& Default)
{
#if VOXEL_DEBUG
	CheckLambdaDoesNotCaptureSharedPtr(MakeWeakPtr(Ptr).Pin(), Lambda);
#endif

	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(
		Ptr,
		MoveTemp(Lambda),
		MoveTemp(Default));
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

template<
	typename T,
	typename LambdaType,
	typename ReturnType = LambdaReturnType_T<LambdaType>,
	typename = std::enable_if_t<
		(TIsDerivedFrom<T, UObject>::Value || TIsDerivedFrom<T, IInterface>::Value) &&
		std::is_void_v<ReturnType>>>
FORCEINLINE auto MakeWeakObjectPtrLambda(T* Ptr, LambdaType Lambda)
{
	return TMakeWeakObjectPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(Ptr, MoveTemp(Lambda));
}

template<
	typename T,
	typename LambdaType,
	typename ReturnType = LambdaReturnType_T<LambdaType>,
	typename = std::enable_if_t<
		(TIsDerivedFrom<T, UObject>::Value || TIsDerivedFrom<T, IInterface>::Value) &&
		!std::is_void_v<ReturnType>>>
FORCEINLINE auto MakeWeakObjectPtrLambda(T* Ptr, LambdaType Lambda, ReturnType&& Default = {})
{
	return TMakeWeakObjectPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(Ptr, MoveTemp(Lambda), MoveTemp(Default));
}

template<
	typename T,
	typename LambdaType,
	typename = std::enable_if_t<TIsDerivedFrom<T, UObject>::Value || TIsDerivedFrom<T, IInterface>::Value>>
FORCEINLINE auto MakeWeakObjectPtrDelegate(T* Ptr, LambdaType Lambda)
{
	return TDelegate<LambdaSignature_T<LambdaType>>::CreateWeakLambda(Ptr, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename>
struct TMakeStrongPtrLambdaHelper;

template<typename... ArgTypes, typename ReturnType>
struct TMakeStrongPtrLambdaHelper<TVoxelTypes<ArgTypes...>, ReturnType>
{
	template<typename T, typename LambdaType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda)
	{
		return [StrongPtr = MakeSharedRef(Ptr), Lambda = MoveTemp(Lambda)](ArgTypes... Args) -> ReturnType
		{
			(void)StrongPtr;
			return Lambda(Forward<ArgTypes>(Args)...);
		};
	}
};

template<typename T, typename LambdaType>
FORCEINLINE auto MakeStrongPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeStrongPtrLambdaHelper<LambdaArgTypes_T<LambdaType>, LambdaReturnType_T<LambdaType>>::Make(Ptr, MoveTemp(Lambda));
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
		FVoxelDelegateUtilities::CreateDelegateInstance<TForwardDelegateInstance>(Base, WeakDelegate, MoveTemp(Lambda));
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
	TDelegate<LambdaSignature_T<LambdaType>> Delegate;

	TForwardDelegateInstance<DelegateType, LambdaReturnType_T<LambdaType>, LambdaArgTypes_T<LambdaType>>::Create(
		Delegate,
		MakeSharedCopy(WeakDelegate),
		MoveTemp(Lambda));

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
		FVoxelDelegateUtilities::CreateDelegateInstance<TMulticastForwardDelegateInstance>(Base, Delegate);
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