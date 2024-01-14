// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Utilities/VoxelObjectUtilities.h"
#include "VoxelMinimal/VoxelDuplicateTransient.h"
#include "VoxelVirtualStruct.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelVirtualStruct
#if CPP
	: public FVirtualDestructor
#endif
{
	GENERATED_BODY()
	VOXEL_COUNT_INSTANCES()

public:
	FVoxelVirtualStruct() = default;

	virtual FString Internal_GetMacroName() const;
	virtual UScriptStruct* Internal_GetStruct() const
	{
		return StaticStruct();
	}
	virtual void Internal_UpdateWeakReferenceInternal(const TSharedPtr<FVoxelVirtualStruct>& SharedPtr)
	{
	}

	// Only called if this is stored in an instanced struct
	virtual void PreSerialize() {}
	virtual void PostSerialize() {}

	FORCEINLINE UScriptStruct* GetStruct() const
	{
		if (!PrivateStruct.Get())
		{
			PrivateStruct = Internal_GetStruct();
		}

		checkfVoxelSlow(
			PrivateStruct.Get() == Internal_GetStruct() || PrivateStruct.Get()->GetName().StartsWith("LIVECODING_"),
			TEXT("GetStruct() called in %s::%s"),
			*PrivateStruct.Get()->GetStructCPPName(),
			*PrivateStruct.Get()->GetStructCPPName());

		return PrivateStruct.Get();
	}

	FORCEINLINE bool IsA(const UScriptStruct* Struct) const
	{
		return GetStruct()->IsChildOf(Struct);
	}
	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, FVoxelVirtualStruct>::Value>::Type>
	FORCEINLINE bool IsA() const
	{
		return this->IsA(T::StaticStruct());
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, FVoxelVirtualStruct>::Value>::Type>
	T* As()
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return static_cast<T*>(this);
	}
	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, FVoxelVirtualStruct>::Value>::Type>
	const T* As() const
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return static_cast<const T*>(this);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, FVoxelVirtualStruct>::Value>::Type>
	T& AsChecked()
	{
		checkVoxelSlow(IsA<T>());
		return static_cast<T&>(*this);
	}
	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, FVoxelVirtualStruct>::Value>::Type>
	const T& AsChecked() const
	{
		checkVoxelSlow(IsA<T>());
		return static_cast<const T&>(*this);
	}

	void CopyTo(FVoxelVirtualStruct& Other) const;
	TSharedRef<FVoxelVirtualStruct> MakeSharedCopy() const;
	void AddStructReferencedObjects(FReferenceCollector& Collector);

	bool Equals_UPropertyOnly(
		const FVoxelVirtualStruct& Other,
		bool bIgnoreTransient = true) const;

	template<typename T>
	FORCEINLINE TVoxelPropertyValueIteratorRangedFor<T> CreatePropertyValueIterator()
	{
		return ::CreatePropertyValueIterator<T>(GetStruct(), this);
	}

protected:
	template<typename T>
	static void Internal_UpdateWeakReferenceInternal(
		const TSharedPtr<FVoxelVirtualStruct>& SharedPtr,
		T* Object)
	{
		if constexpr (IsDerivedFromSharedFromThis<T>())
		{
			Object->UpdateWeakReferenceInternal(&ReinterpretCastRef<const TSharedPtr<T>>(SharedPtr), static_cast<T*>(SharedPtr.Get()));
		}
	}

private:
	mutable TVoxelDuplicateTransient<UScriptStruct*> PrivateStruct;
};

#define GENERATED_VIRTUAL_STRUCT_BODY_ALIASES() \
	auto MakeSharedCopy() const -> decltype(auto) \
	{ \
		const TSharedRef<VOXEL_THIS_TYPE> SharedRef = StaticCastSharedRef<VOXEL_THIS_TYPE>(Super::MakeSharedCopy()); \
		SharedPointerInternals::EnableSharedFromThis(&SharedRef, &SharedRef.Get(), &SharedRef.Get()); \
		return SharedRef; \
	} \
	template<typename T, typename = typename TEnableIf<!TIsReferenceType<T>::Value>::Type> \
	static auto MakeSharedCopy(T&& Data) -> decltype(auto) \
	{ \
		return ::MakeSharedCopy(MoveTemp(Data)); \
	} \
	template<typename T> \
	static auto MakeSharedCopy(const T& Data) -> decltype(auto) \
	{ \
		return ::MakeSharedCopy(Data); \
	}

#define GENERATED_VIRTUAL_STRUCT_BODY_IMPL(Parent) \
	virtual UScriptStruct* PREPROCESSOR_JOIN(Internal_GetStruct, Parent)() const override { return StaticStruct(); } \
	virtual void Internal_UpdateWeakReferenceInternal(const TSharedPtr<FVoxelVirtualStruct>& SharedPtr) \
	{ \
		FVoxelVirtualStruct::Internal_UpdateWeakReferenceInternal(SharedPtr, this); \
	} \
	GENERATED_VIRTUAL_STRUCT_BODY_ALIASES()

#define DECLARE_VIRTUAL_STRUCT_PARENT(Parent, Macro) \
	virtual FString Internal_GetMacroName() const override \
	{ \
		return #Macro; \
	} \
	virtual UScriptStruct* Internal_GetStruct() const final override \
	{ \
		return Internal_GetStruct ## Parent(); \
	} \
	virtual UScriptStruct* Internal_GetStruct ## Parent() const \
	{ \
		return StaticStruct(); \
	} \
	GENERATED_VIRTUAL_STRUCT_BODY_ALIASES()

#define GENERATED_VIRTUAL_STRUCT_BODY() GENERATED_VIRTUAL_STRUCT_BODY_IMPL(PREPROCESSOR_NOTHING)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To>
FORCEINLINE To* Cast(FVoxelVirtualStruct* Struct)
{
	if (!Struct ||
		!Struct->IsA<To>())
	{
		return nullptr;
	}

	return static_cast<To*>(Struct);
}
template<typename To>
FORCEINLINE const To* Cast(const FVoxelVirtualStruct* Struct)
{
	return Cast<To>(ConstCast(Struct));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To>
FORCEINLINE To* Cast(FVoxelVirtualStruct& Struct)
{
	return Cast<To>(&Struct);
}
template<typename To>
FORCEINLINE const To* Cast(const FVoxelVirtualStruct& Struct)
{
	return Cast<To>(&Struct);
}

template<typename To>
FORCEINLINE To& CastChecked(FVoxelVirtualStruct& Struct)
{
	checkVoxelSlow(Struct.IsA<To>());
	return static_cast<To&>(Struct);
}
template<typename To>
FORCEINLINE const To& CastChecked(const FVoxelVirtualStruct& Struct)
{
	return CastChecked<To>(ConstCast(Struct));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<To> Cast(const TSharedPtr<From>& Struct)
{
	if (!Struct ||
		!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedPtr<To>(Struct);
}
template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<const To> Cast(const TSharedPtr<const From>& Struct)
{
	if (!Struct ||
		!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedPtr<const To>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<To> Cast(const TSharedRef<From>& Struct)
{
	if (!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedRef<To>(Struct);
}
template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<const To> Cast(const TSharedRef<const From>& Struct)
{
	if (!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedRef<const To>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedRef<To> CastChecked(const TSharedRef<From>& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return StaticCastSharedRef<To>(Struct);
}
template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedRef<const To> CastChecked(const TSharedRef<const From>& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return StaticCastSharedRef<const To>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TUniquePtr<To> CastChecked(TUniquePtr<From>&& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return TUniquePtr<To>(static_cast<To*>(Struct.Release()));
}
template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TUniquePtr<const To> CastChecked(TUniquePtr<const From>&& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return TUniquePtr<const To>(static_cast<const To*>(Struct.Release()));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<To> CastEnsured(const TSharedPtr<From>& Struct)
{
	const TSharedPtr<To> Result = Cast<To>(Struct);
	ensure(!Struct || Result);
	return Result;
}
template<typename To, typename From, typename = typename TEnableIf<TIsDerivedFrom<From, FVoxelVirtualStruct>::Value>::Type>
FORCEINLINE TSharedPtr<const To> CastEnsured(const TSharedPtr<const From>& Struct)
{
	const TSharedPtr<const To> Result = Cast<const To>(Struct);
	ensure(!Struct || Result);
	return Result;
}