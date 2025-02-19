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
	template<typename T>
	requires std::derived_from<T, FVoxelVirtualStruct>
	FORCEINLINE bool IsA() const
	{
		return this->IsA(T::StaticStruct());
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

	void CopyTo(FVoxelVirtualStruct& Other) const;
	TSharedRef<FVoxelVirtualStruct> MakeSharedCopy() const;

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

#if VOXEL_ENGINE_VERSION >= 505
#define VOXEL_ENABLE_SHARED_FROM_THIS(Name) \
	SharedPointerInternals::EnableSharedFromThis(&Name, &Name.Get());
#else
#define VOXEL_ENABLE_SHARED_FROM_THIS(Name) \
	SharedPointerInternals::EnableSharedFromThis(&Name, &Name.Get(), &Name.Get());
#endif

#define GENERATED_VIRTUAL_STRUCT_BODY_ALIASES() \
	FORCEINLINE auto MakeSharedCopy() const -> decltype(auto) \
	{ \
		const TSharedRef<VOXEL_THIS_TYPE> SharedRef = StaticCastSharedRef<VOXEL_THIS_TYPE>(Super::MakeSharedCopy()); \
		VOXEL_ENABLE_SHARED_FROM_THIS(SharedRef); \
		return SharedRef; \
	} \
	template<typename T, typename = std::enable_if_t<!TIsReferenceType<T>::Value>> \
	FORCEINLINE static auto MakeSharedCopy(T&& Data) -> decltype(auto) \
	{ \
		return ::MakeSharedCopy(MoveTemp(Data)); \
	} \
	template<typename T> \
	FORCEINLINE static auto MakeSharedCopy(const T& Data) -> decltype(auto) \
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

#define VOXEL_CAST_CHECK \
	template< \
		typename To, \
		typename From, \
		typename = std::enable_if_t<!std::is_const_v<From>>, \
		typename = std::enable_if_t<!std::derived_from<From, UObject>>, \
		typename = std::enable_if_t<std::derived_from<From, FVoxelVirtualStruct>>, \
		typename = std::enable_if_t<std::derived_from<To, From>>>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_CAST_CHECK
FORCEINLINE To* CastStruct(From* Struct)
{
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