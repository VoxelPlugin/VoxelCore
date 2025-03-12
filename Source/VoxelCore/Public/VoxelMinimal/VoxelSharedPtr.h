// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMacros.h"

template<typename T, typename = decltype(DeclVal<T>().AsWeak())>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(T* Ptr)
{
	return StaticCastWeakPtr<T>(Ptr->AsWeak());
}
template<typename T, typename = decltype(DeclVal<T>().AsWeak())>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(T& Ptr)
{
	return StaticCastWeakPtr<T>(Ptr.AsWeak());
}

template<typename T, typename = decltype(DeclVal<T>().AsShared())>
FORCEINLINE TSharedRef<T> MakeSharedRef(T* Ptr)
{
	return StaticCastSharedRef<T>(Ptr->AsShared());
}
template<typename T, typename = decltype(DeclVal<T>().AsShared())>
FORCEINLINE TSharedRef<T> MakeSharedRef(T& Ptr)
{
	return StaticCastSharedRef<T>(Ptr.AsShared());
}
template<typename T>
FORCEINLINE TSharedRef<T> MakeSharedRef(const TSharedRef<T>& Ref)
{
	return Ref;
}

template<typename T>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(const TSharedPtr<T>& Ptr)
{
	return TWeakPtr<T>(Ptr);
}
template<typename T>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(const TSharedRef<T>& Ptr)
{
	return TWeakPtr<T>(Ptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Need std::enable_if_t as &&& is equivalent to &, so T could get matched with Smthg&
template<typename T>
FORCEINLINE std::enable_if_t<!TIsReferenceType<T>::Value, TSharedRef<T>> MakeSharedCopy(T&& Data)
{
	return MakeShared<T>(MoveTemp(Data));
}
template<typename T>
FORCEINLINE std::enable_if_t<!TIsReferenceType<T>::Value, TUniquePtr<T>> MakeUniqueCopy(T&& Data)
{
	return MakeUnique<T>(MoveTemp(Data));
}

template<typename T>
FORCEINLINE TSharedRef<T> MakeSharedCopy(const T& Data)
{
	return MakeShared<T>(Data);
}
template<typename T>
FORCEINLINE TUniquePtr<T> MakeUniqueCopy(const T& Data)
{
	return MakeUnique<T>(Data);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE SharedPointerInternals::TRawPtrProxy<T> MakeShareable(TUniquePtr<T> UniquePtr)
{
	checkVoxelSlow(UniquePtr.IsValid());
	return MakeShareable(UniquePtr.Release());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelNullSharedRef
{
	template<typename T>
	FORCEINLINE operator TSharedRef<T>() const
	{
		return ReinterpretCastRef<TSharedRef<T>>(TSharedPtr<T>());
	}
};
constexpr FVoxelNullSharedRef SharedRef_Null;

template<typename T>
FORCEINLINE TSharedRef<T> ToSharedRefFast(TSharedPtr<T>&& SharedPtr)
{
	checkVoxelSlow(SharedPtr.IsValid());
	return ReinterpretCastRef<TSharedRef<T>>(SharedPtr);
}
template<typename T>
FORCEINLINE const TSharedRef<T>& ToSharedRefFast(const TSharedPtr<T>& SharedPtr)
{
	checkVoxelSlow(SharedPtr.IsValid());
	return ReinterpretCastRef<TSharedRef<T>>(SharedPtr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE T* GetWeakPtrObject_Unsafe(const TWeakPtr<T>& WeakPtr)
{
	struct FWeakPtr
	{
		T* Object;
		SharedPointerInternals::FWeakReferencer<ESPMode::ThreadSafe> WeakReferenceCount;
	};
	return ReinterpretCastRef<FWeakPtr>(WeakPtr).Object;
}

template<typename T>
FORCEINLINE const SharedPointerInternals::FWeakReferencer<ESPMode::ThreadSafe>& GetWeakPtrWeakReferencer(const TWeakPtr<T>& WeakPtr)
{
	struct FWeakPtr
	{
		T* Object;
		SharedPointerInternals::FWeakReferencer<ESPMode::ThreadSafe> WeakReferencer;
	};
	return ReinterpretCastRef<FWeakPtr>(WeakPtr).WeakReferencer;
}
FORCEINLINE SharedPointerInternals::TReferenceControllerBase<ESPMode::ThreadSafe>* GetWeakReferencerReferenceController(const SharedPointerInternals::FWeakReferencer<ESPMode::ThreadSafe>& WeakReferencer)
{
	return ReinterpretCastRef<SharedPointerInternals::TReferenceControllerBase<ESPMode::ThreadSafe>*>(WeakReferencer);
}

template<typename T>
FORCEINLINE const TWeakPtr<T>& GetSharedFromThisWeakPtr(const TSharedFromThis<T>& SharedFromThis)
{
	struct FSharedFromThis
	{
		TWeakPtr<T> WeakPtr;
	};
	return ReinterpretCastRef<FSharedFromThis>(SharedFromThis).WeakPtr;
}

template<typename T>
FORCEINLINE bool IsExplicitlyNull(const TWeakPtr<T>& WeakPtr)
{
	return GetWeakPtrObject_Unsafe(WeakPtr) == nullptr;
}

template<typename T>
FORCEINLINE bool IsSharedFromThisUnique(const TSharedFromThis<T>& SharedFromThis)
{
	const TWeakPtr<T>& WeakPtr = GetSharedFromThisWeakPtr(SharedFromThis);
	const SharedPointerInternals::FWeakReferencer<ESPMode::ThreadSafe>& WeakReferencer = GetWeakPtrWeakReferencer(WeakPtr);
	const SharedPointerInternals::TReferenceControllerBase<ESPMode::ThreadSafe>* ReferenceController = GetWeakReferencerReferenceController(WeakReferencer);
	checkVoxelSlow(ReferenceController);

	const int32 ReferenceCount = ReferenceController->GetSharedReferenceCount();
	checkVoxelSlow(ReferenceCount >= 1);
	return ReferenceCount == 1;
}

template<typename T, typename LambdaType>
FORCEINLINE TSharedRef<T> MakeShareable_CustomDestructor(T* Object, LambdaType&& Destructor)
{
	return ReinterpretCastRef<TSharedRef<T>>(TSharedPtr<T>(
		Object,
		[Object, OnDestroy = MoveTemp(Destructor)](T* InObject)
		{
			if (!ensure(Object == InObject))
			{
				return;
			}

			OnDestroy();
		}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T, typename LambdaType>
void CheckLambdaDoesNotCaptureSharedPtr(
	const TSharedPtr<T>& SharedPtr,
	const LambdaType& Lambda)
{
	if (!SharedPtr)
	{
		return;
	}

	if constexpr (std::is_copy_constructible_v<LambdaType>)
	{
		const auto IsValid = [&]
		{
			const int32 CountBefore = SharedPtr.GetSharedReferenceCount();
			LambdaType LambdaCopy = Lambda;
			const int32 CountAfter = SharedPtr.GetSharedReferenceCount();

			return CountBefore == CountAfter;
		};

		for (int32 Index = 0; Index < 1000; Index++)
		{
			if (IsValid())
			{
				return;
			}
		}

		ensureMsgf(false, TEXT("SharedPtr used in MakeWeakPtrLambda should not be captured. Pass it by ref or use MakeStrongPtrLambda"));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::Internal
{
	struct FVoidPtr;
}

using FSharedVoidPtr = TSharedPtr<Voxel::Internal::FVoidPtr>;
using FSharedVoidRef = TSharedRef<Voxel::Internal::FVoidPtr>;
using FWeakVoidPtr = TWeakPtr<Voxel::Internal::FVoidPtr>;

template<typename T>
FORCEINLINE const FWeakVoidPtr& MakeWeakVoidPtr(const TWeakPtr<T>& Ptr)
{
	return ReinterpretCastRef<FWeakVoidPtr>(Ptr);
}
template<typename T>
FORCEINLINE const FSharedVoidPtr& MakeSharedVoidPtr(const TSharedPtr<T>& Ptr)
{
	return ReinterpretCastRef<FSharedVoidPtr>(Ptr);
}
template<typename T>
FORCEINLINE const FSharedVoidRef& MakeSharedVoidRef(const TSharedRef<T>& Ptr)
{
	return ReinterpretCastRef<FSharedVoidRef>(Ptr);
}
FORCEINLINE FSharedVoidRef MakeSharedVoid()
{
	return MakeSharedVoidRef(MakeShared<int32>());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Stock MakeShared creates hard to fix errors
#if VOXEL_DEV_WORKFLOW && !INTELLISENSE_PARSER

// Hack to work with classes doing friend class SharedPointerInternals::TIntrusiveReferenceController
namespace SharedPointerInternals
{
	struct FMakeSharedDummy;

	template<>
	class TIntrusiveReferenceController<FMakeSharedDummy, ESPMode::ThreadSafe>
	{
	public:
		template<typename ObjectType, typename... ArgTypes>
		static void Call(decltype(ObjectType(DeclVal<ArgTypes>()...))* = nullptr);
	};

	template<typename ObjectType, typename... ArgTypes>
	concept CanMakeShared = requires
	{
		TIntrusiveReferenceController<FMakeSharedDummy, ESPMode::ThreadSafe>::Call<ObjectType, ArgTypes...>();
	};
}

template<typename InObjectType, ESPMode InMode = ESPMode::ThreadSafe, typename... InArgTypes>
requires SharedPointerInternals::CanMakeShared<InObjectType, InArgTypes&&...>
[[nodiscard]] FORCEINLINE TSharedRef<InObjectType, InMode> MakeShared_Safe(InArgTypes&&... Args)
{
	return MakeShared<InObjectType, InMode>(Forward<InArgTypes>(Args)...);
}

#define MakeShared MakeShared_Safe
#endif