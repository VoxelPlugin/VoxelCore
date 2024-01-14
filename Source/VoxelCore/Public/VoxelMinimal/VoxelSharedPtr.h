// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMacros.h"
#include "VoxelMemory.h"

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

template<typename T>
FORCEINLINE typename TEnableIf<!TIsReferenceType<T>::Value, T>::Type MakeCopy(T&& Data)
{
	return MoveTemp(Data);
}
template<typename T>
FORCEINLINE T MakeCopy(const T& Data)
{
	return Data;
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
template<typename T>
FORCEINLINE SharedPointerInternals::TRawPtrProxy<T> MakeShareable(TVoxelUniquePtr<T> UniquePtr)
{
	checkVoxelSlow(UniquePtr.IsValid());
	return MakeVoxelShareable(UniquePtr.Release());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE TSharedRef<T> MakeNullSharedRef()
{
	return ReinterpretCastRef<TSharedRef<T>>(TSharedPtr<T>());
}

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

// Useful when creating shared ptrs that are supposed to never expire, typically for default shared values
template<typename T>
FORCEINLINE void ClearSharedPtrReferencer(TSharedPtr<T>& Ptr)
{
	struct FSharedPtr
	{
		void* Object;
		void* Referencer;
	};
	ReinterpretCastRef<FSharedPtr>(Ptr).Referencer = nullptr;
}
template<typename T>
FORCEINLINE void ClearSharedRefReferencer(TSharedRef<T>& Ptr)
{
	ClearSharedPtrReferencer(ReinterpretCastRef<TSharedPtr<T>>(Ptr));
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
FORCEINLINE FWeakVoidPtr MakeWeakVoidPtr(const TWeakPtr<T>& Ptr)
{
	return ReinterpretCastRef<FWeakVoidPtr>(Ptr);
}
template<typename T>
FORCEINLINE FSharedVoidPtr MakeSharedVoidPtr(const TSharedPtr<T>& Ptr)
{
	return ReinterpretCastRef<FSharedVoidPtr>(Ptr);
}
template<typename T>
FORCEINLINE FSharedVoidRef MakeSharedVoidRef(const TSharedRef<T>& Ptr)
{
	return ReinterpretCastRef<FSharedVoidRef>(Ptr);
}
FORCEINLINE FSharedVoidRef MakeSharedVoid()
{
	return MakeSharedVoidRef(MakeVoxelShared<int32>());
}