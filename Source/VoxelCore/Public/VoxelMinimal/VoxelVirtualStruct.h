// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelDuplicateTransient.h"
#include "Utilities/VoxelObjectUtilities.h"
#include "VoxelVirtualStruct.generated.h"

class FJsonObject;

USTRUCT()
struct VOXELCORE_API FVoxelVirtualStruct
{
	GENERATED_BODY()
	VOXEL_COUNT_INSTANCES()

public:
	FVoxelVirtualStruct() = default;
	virtual ~FVoxelVirtualStruct() = default;

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
	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE bool IsA() const
	{
		if constexpr (std::is_final_v<T>)
		{
			return GetStruct() == StaticStructFast<T>();
		}

		return this->IsA(StaticStructFast<T>());
	}

	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE T* As()
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return static_cast<T*>(this);
	}
	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE const T* As() const
	{
		if (!IsA<T>())
		{
			return nullptr;
		}

		return static_cast<const T*>(this);
	}

	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE T& AsChecked()
	{
		checkVoxelSlow(IsA<T>());
		return static_cast<T&>(*this);
	}
	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE const T& AsChecked() const
	{
		checkVoxelSlow(IsA<T>());
		return static_cast<const T&>(*this);
	}

	TSharedRef<FVoxelVirtualStruct> MakeSharedCopy_Generic() const;

public:
	TSharedRef<FJsonObject> SaveToJson(
		EPropertyFlags CheckFlags = CPF_None,
		EPropertyFlags SkipFlags = CPF_None) const;

	bool LoadFromJson(
		const TSharedRef<FJsonObject>& JsonObject,
		bool bStrictMode = false,
		EPropertyFlags CheckFlags = CPF_None,
		EPropertyFlags SkipFlags = CPF_None);

public:
	bool Equals_UPropertyOnly(
		const FVoxelVirtualStruct& Other,
		bool bIgnoreTransient = true) const;

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define IMPL_GENERATED_VIRTUAL_STRUCT_BODY_MAKE_SHARED_COPY() \
	FORCEINLINE auto MakeSharedCopy() const -> decltype(auto) \
	{ \
		checkStatic(std::is_copy_assignable_v<VOXEL_THIS_TYPE>); \
		\
		if constexpr (std::is_final_v<VOXEL_THIS_TYPE>) \
		{ \
			if constexpr (std::is_copy_constructible_v<VOXEL_THIS_TYPE>) \
			{ \
				return ::MakeSharedCopy(*this); \
			} \
			else \
			{ \
				TSharedRef<VOXEL_THIS_TYPE> SharedRef = MakeShared<VOXEL_THIS_TYPE>(); \
				*SharedRef = *this; \
				return SharedRef; \
			} \
		} \
		\
		TSharedRef<VOXEL_THIS_TYPE> SharedRef = StaticCastSharedRef<VOXEL_THIS_TYPE>(MakeSharedCopy_Generic()); \
		SharedPointerInternals::EnableSharedFromThis(&SharedRef, &SharedRef.Get()); \
		return SharedRef; \
	} \
	template<typename T> \
	requires (!std::is_reference_v<T>) \
	FORCEINLINE static auto MakeSharedCopy(T&& Data) -> decltype(auto) \
	{ \
		return ::MakeSharedCopy(MoveTemp(Data)); \
	} \
	template<typename T> \
	FORCEINLINE static auto MakeSharedCopy(const T& Data) -> decltype(auto) \
	{ \
		return ::MakeSharedCopy(Data); \
	}

#define GENERATED_VIRTUAL_STRUCT_BODY_NO_COPY(Parent) \
	virtual UScriptStruct* PREPROCESSOR_JOIN(Internal_GetStruct, Parent)() const override { return StaticStruct(); } \
	virtual void Internal_UpdateWeakReferenceInternal(const TSharedPtr<FVoxelVirtualStruct>& SharedPtr) \
	{ \
		FVoxelVirtualStruct::Internal_UpdateWeakReferenceInternal(SharedPtr, this); \
	}

#define DECLARE_VIRTUAL_STRUCT_PARENT_NO_COPY(Parent, Macro) \
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
	}

#define GENERATED_VIRTUAL_STRUCT_BODY(Parent) \
	GENERATED_VIRTUAL_STRUCT_BODY_NO_COPY(Parent) \
	IMPL_GENERATED_VIRTUAL_STRUCT_BODY_MAKE_SHARED_COPY()

#define DECLARE_VIRTUAL_STRUCT_PARENT(Parent, Macro) \
	DECLARE_VIRTUAL_STRUCT_PARENT_NO_COPY(Parent, Macro) \
	IMPL_GENERATED_VIRTUAL_STRUCT_BODY_MAKE_SHARED_COPY()

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_CAST_CHECK \
	template<typename To, typename From> \
	requires \
	( \
		!std::is_const_v<From> && \
		std::derived_from<From, FVoxelVirtualStruct> && \
		std::derived_from<To, From> \
	)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE To* CastStruct(From* Struct)
{
	if constexpr (std::is_same_v<To, From>)
	{
		return Struct;
	}

	if (!Struct ||
		!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return static_cast<To*>(Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE const To* CastStruct(const From* Struct)
{
	if constexpr (std::is_same_v<To, From>)
	{
		return Struct;
	}

	if (!Struct ||
		!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return static_cast<const To*>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE To* CastStruct(From& Struct)
{
	return CastStruct<To>(&Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE const To* CastStruct(const From& Struct)
{
	return CastStruct<To>(&Struct);
}

VOXEL_CAST_CHECK
FORCEINLINE To& CastStructChecked(From& Struct)
{
	checkVoxelSlow(Struct.template IsA<To>());
	return static_cast<To&>(Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE const To& CastStructChecked(const From& Struct)
{
	checkVoxelSlow(Struct.template IsA<To>());
	return static_cast<const To&>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<To> CastStruct(const TSharedPtr<From>& Struct)
{
	if (!Struct ||
		!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedPtr<To>(Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<const To> CastStruct(const TSharedPtr<const From>& Struct)
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

VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<To> CastStruct(const TSharedRef<From>& Struct)
{
	if (!Struct->template IsA<To>())
	{
		return nullptr;
	}

	return StaticCastSharedRef<To>(Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<const To> CastStruct(const TSharedRef<const From>& Struct)
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

VOXEL_CAST_CHECK
FORCEINLINE TSharedRef<To> CastStructChecked(const TSharedRef<From>& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return StaticCastSharedRef<To>(Struct);
}
VOXEL_CAST_CHECK
FORCEINLINE TSharedRef<const To> CastStructChecked(const TSharedRef<const From>& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return StaticCastSharedRef<const To>(Struct);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE TUniquePtr<To> CastStructChecked(TUniquePtr<From>&& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return TUniquePtr<To>(static_cast<To*>(Struct.Release()));
}
VOXEL_CAST_CHECK
FORCEINLINE TUniquePtr<const To> CastStructChecked(TUniquePtr<const From>&& Struct)
{
	checkVoxelSlow(Struct->template IsA<To>());
	return TUniquePtr<const To>(static_cast<const To*>(Struct.Release()));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<To> CastStructEnsured(const TSharedRef<From>& Struct)
{
	const TSharedPtr<To> Result = CastStruct<To>(Struct);
	ensure(Result);
	return Result;
}
VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<To> CastStructEnsured(const TSharedPtr<From>& Struct)
{
	const TSharedPtr<To> Result = CastStruct<To>(Struct);
	ensure(!Struct || Result);
	return Result;
}
VOXEL_CAST_CHECK
FORCEINLINE TSharedPtr<const To> CastStructEnsured(const TSharedPtr<const From>& Struct)
{
	const TSharedPtr<const To> Result = CastStruct<const To>(Struct);
	ensure(!Struct || Result);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#undef VOXEL_CAST_CHECK