// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "UObject/UObjectHash.h"
#include "UObject/WeakInterfacePtr.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/VoxelObjectPtr.h"
#include "VoxelMinimal/VoxelStructView.h"
#include "VoxelMinimal/VoxelDereferencingIterator.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"

template<typename>
struct TIsSoftObjectPtr
{
	static constexpr bool Value = false;
};

template<>
struct TIsSoftObjectPtr<FSoftObjectPtr>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsSoftObjectPtr<TSoftObjectPtr<T>>
{
	static constexpr bool Value = true;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename = void>
struct TIsObjectPtr
{
	static constexpr bool Value = false;
};

template<typename T>
struct TIsObjectPtr<T, std::enable_if_t<std::derived_from<T, UObject>>>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsObjectPtr<T*, std::enable_if_t<std::derived_from<T, UObject>>>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsObjectPtr<TObjectPtr<T>>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsObjectPtr<TObjectKey<T>>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsObjectPtr<TWeakObjectPtr<T>>
{
	static constexpr bool Value = true;
};

template<typename T>
struct TIsObjectPtr<TVoxelObjectPtr<T>>
{
	static constexpr bool Value = true;
};

template<typename T>
static constexpr bool IsObjectPtr = TIsObjectPtr<T>::Value;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename, typename = void>
struct TObjectPtrInnerType;

template<typename T>
struct TObjectPtrInnerType<T, std::enable_if_t<std::derived_from<T, UObject>>>
{
	using Type = T;
};

template<typename T>
struct TObjectPtrInnerType<T*, std::enable_if_t<std::derived_from<T, UObject>>>
{
	using Type = T;
};

template<typename T>
struct TObjectPtrInnerType<TObjectPtr<T>>
{
	using Type = T;
};

template<typename T>
struct TObjectPtrInnerType<TObjectKey<T>>
{
	using Type = T;
};

template<typename T>
struct TObjectPtrInnerType<TWeakObjectPtr<T>>
{
	using Type = T;
};

template<typename T>
struct TObjectPtrInnerType<TVoxelObjectPtr<T>>
{
	using Type = T;
};

template<typename T>
using ObjectPtrInnerType = typename TObjectPtrInnerType<T>::Type;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename>
struct TSubclassOfType;

template<typename T>
struct TSubclassOfType<TSubclassOf<T>>
{
	using Type = T;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename FieldType>
FORCEINLINE FieldType* CastField(FField& Src)
{
	return CastField<FieldType>(&Src);
}
template<typename FieldType>
FORCEINLINE const FieldType* CastField(const FField& Src)
{
	return CastField<FieldType>(&Src);
}

template<typename FieldType>
FORCEINLINE FieldType& CastFieldChecked(FField& Src)
{
	return *CastFieldChecked<FieldType>(&Src);
}
template<typename FieldType>
FORCEINLINE const FieldType& CastFieldChecked(const FField& Src)
{
	return *CastFieldChecked<FieldType>(&Src);
}

template<typename To, typename From>
FORCEINLINE To* CastEnsured(From* Src)
{
	To* Result = Cast<To>(Src);
	ensure(!Src || Result);
	return Result;
}
template<typename To, typename From>
FORCEINLINE const To* CastEnsured(const From* Src)
{
	const To* Result = Cast<To>(Src);
	ensure(!Src || Result);
	return Result;
}
template<typename To, typename From>
FORCEINLINE To* CastEnsured(const TObjectPtr<From>& Src)
{
	return CastEnsured<To>(Src.Get());
}

template<typename ToType, typename FromType, typename Allocator, typename = std::enable_if_t<std::derived_from<
	std::remove_const_t<ToType>,
	std::remove_const_t<FromType>>>>
FORCEINLINE const TArray<ToType*, Allocator>& CastChecked(const TArray<FromType*, Allocator>& Array)
{
#if VOXEL_DEBUG
	for (FromType* Element : Array)
	{
		checkVoxelSlow(Element->template IsA<ToType>());
	}
#endif

	return ReinterpretCastArray<ToType*>(Array);
}

template<typename ToType, typename FromType, typename = std::enable_if_t<
	std::derived_from<
		std::remove_const_t<ToType>,
		UObject
	>
	&&
	std::derived_from<
		std::remove_const_t<ToType>,
		std::remove_const_t<FromType>
	>>>
FORCEINLINE std::conditional_t<std::is_const_v<FromType>, const ToType, ToType>& CastChecked(FromType& Object)
{
	return *CastChecked<ToType>(&Object);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Class>
FORCEINLINE FName GetClassFName()
{
	// StaticClass is a few instructions for FProperties
	static FName StaticName;
	if (StaticName.IsNone())
	{
		StaticName = Class::StaticClass()->GetFName();
	}
	return StaticName;
}
template<typename Class>
FORCEINLINE FString GetClassName()
{
	return Class::StaticClass()->GetName();
}

VOXELCORE_API UScriptStruct* FindCoreStruct(const TCHAR* Name);

template<typename Enum>
FORCEINLINE UEnum* StaticEnumFast()
{
	VOXEL_STATIC_HELPER(UEnum*)
	{
		StaticValue = StaticEnum<std::decay_t<Enum>>();
	}
	return StaticValue;
}
template<typename Struct>
FORCEINLINE UScriptStruct* StaticStructFast()
{
	VOXEL_STATIC_HELPER(UScriptStruct*)
	{
		if constexpr (std::is_same_v<Struct, FVector2f>)
		{
			StaticValue = FindCoreStruct(TEXT("Vector2f"));
		}
		else if constexpr (std::is_same_v<Struct, FVector3f>)
		{
			StaticValue = FindCoreStruct(TEXT("Vector3f"));
		}
		else if constexpr (std::is_same_v<Struct, FVector4f>)
		{
			StaticValue = FindCoreStruct(TEXT("Vector4f"));
		}
		else if constexpr (std::is_same_v<Struct, FLinearColor>)
		{
			StaticValue = FindCoreStruct(TEXT("LinearColor"));
		}
		else if constexpr (std::is_same_v<Struct, FQuat4f>)
		{
			StaticValue = FindCoreStruct(TEXT("Quat4f"));
		}
		else
		{
			StaticValue = TBaseStructure<std::decay_t<Struct>>::Get();
		}
	}
	return StaticValue;
}
template<typename Class>
FORCEINLINE UClass* StaticClassFast()
{
	VOXEL_STATIC_HELPER(UClass*)
	{
		StaticValue = Class::StaticClass();
	}
	return StaticValue;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API void ForEachAssetDataOfClass(
	const UClass* ClassToLookFor,
	TFunctionRef<void(const FAssetData&)> Operation);

VOXELCORE_API void ForEachAssetOfClass(
	const UClass* ClassToLookFor,
	TFunctionRef<void(UObject*)> Operation);

template<typename T>
void ForEachAssetDataOfClass(TFunctionRef<void(const FAssetData&)> Operation)
{
	ForEachAssetDataOfClass(T::StaticClass(), Operation);
}

template<typename T, typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
void ForEachAssetOfClass(LambdaType&& Operation)
{
	ForEachAssetOfClass(T::StaticClass(), [&](UObject* Asset)
	{
		Operation(*CastChecked<T>(Asset));
	});
}

template<typename T, typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
void ForEachObjectOfClass(LambdaType&& Operation, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject | RF_MirroredGarbage, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None)
{
	ForEachObjectOfClass(T::StaticClass(), [&](UObject* Object)
	{
		checkVoxelSlow(Object && Object->IsA<T>());
		Operation(*static_cast<T*>(Object));
	}, bIncludeDerivedClasses, ExcludeFlags, ExclusionInternalFlags);
}

template<typename T, typename LambdaType, typename = std::enable_if_t<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>>
void ForEachObjectOfClass_Copy(LambdaType&& Operation, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject | RF_MirroredGarbage, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None)
{
	TVoxelChunkedArray<T*> Objects;
	ForEachObjectOfClass<T>([&](T& Object)
	{
		Objects.Add(&Object);
	}, bIncludeDerivedClasses, ExcludeFlags, ExclusionInternalFlags);

	for (T* Object : Objects)
	{
		Operation(*Object);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T = void, typename ArrayType = std::conditional_t<std::is_void_v<T>, UClass*, TSubclassOf<T>>>
TVoxelArray<ArrayType> GetDerivedClasses(const UClass* BaseClass = T::StaticClass(), const bool bRecursive = true, const bool bRemoveDeprecated = true)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<UClass*> Result;
	GetDerivedClasses(BaseClass, Result, bRecursive);

	if (bRemoveDeprecated)
	{
		Result.RemoveAllSwap([](const UClass* Class)
		{
			return Class->HasAnyClassFlags(CLASS_Deprecated);
		});
	}

	return ReinterpretCastVoxelArray<ArrayType>(MoveTemp(Result));
}

VOXELCORE_API TVoxelArray<UScriptStruct*> GetDerivedStructs(const UScriptStruct* BaseStruct, bool bIncludeBase = false);

template<typename T>
TVoxelArray<UScriptStruct*> GetDerivedStructs(const bool bIncludeBase = false)
{
	return GetDerivedStructs(T::StaticStruct(), bIncludeBase);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API bool IsFunctionInput(const FProperty& Property);
VOXELCORE_API TVoxelArray<UFunction*> GetClassFunctions(const UClass* Class, bool bIncludeSuper = false);

#if WITH_EDITOR
VOXELCORE_API FString GetStringMetaDataHierarchical(const UStruct* Struct, FName Name);
#endif

VOXELCORE_API void ForeachObjectReference(
	UObject& Object,
	TFunctionRef<void(UObject*& ObjectRef)> Lambda,
	EPropertyFlags SkipFlags = CPF_Transient);

VOXELCORE_API void ForeachObjectReference(
	FVoxelStructView Struct,
	TFunctionRef<void(UObject*& ObjectRef)> Lambda,
	EPropertyFlags SkipFlags = CPF_Transient);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct& Struct, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);
VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct* Struct, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);

VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass& Class, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);
VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass* Class, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);

VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction& Function, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);
VOXELCORE_API TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction* Function, EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default);

template<typename T>
FORCEINLINE TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const EFieldIterationFlags IterationFlags = EFieldIterationFlags::Default)
{
	return GetClassProperties(StaticClassFast<T>(), IterationFlags);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FProperty& FindFPropertyChecked_Impl(const FName Name)
{
	UStruct* Struct;
	if constexpr (std::derived_from<T, UObject>)
	{
		Struct = T::StaticClass();
	}
	else
	{
		Struct = StaticStructFast<T>();
	}

	FProperty* Property = FindFProperty<FProperty>(Struct, Name);
	check(Property);
	return *Property;
}

#define FindFPropertyChecked(Class, Name) FindFPropertyChecked_Impl<Class>(GET_MEMBER_NAME_CHECKED(Class, Name))

#define FindUFunctionChecked(Class, Name) \
	[] \
	{ \
		static UFunction* Function = Class::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(Class, Name)); \
		check(Function); \
		return Function; \
	}()

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API FSharedVoidRef MakeSharedStruct(const UScriptStruct* Struct, const void* StructToCopyFrom = nullptr);
VOXELCORE_API FSharedVoidRef MakeShareableStruct(const UScriptStruct* Struct, void* StructMemory);

template<typename T>
TSharedRef<T> MakeSharedStruct(const UScriptStruct* Struct, const T* StructToCopyFrom = nullptr)
{
	checkVoxelSlow(Struct->IsChildOf(StaticStructFast<T>()));

	const TSharedRef<T> SharedRef = ReinterpretCastRef<TSharedRef<T>>(MakeSharedStruct(Struct, static_cast<const void*>(StructToCopyFrom)));
#if VOXEL_ENGINE_VERSION >= 505
	SharedPointerInternals::EnableSharedFromThis(&SharedRef, &SharedRef.Get());
#else
	SharedPointerInternals::EnableSharedFromThis(&SharedRef, &SharedRef.Get(), &SharedRef.Get());
#endif
	return SharedRef;
}

template<typename T, typename = std::enable_if_t<std::has_virtual_destructor_v<T>>>
TUniquePtr<T> MakeUniqueStruct(const UScriptStruct* Struct, const T* StructToCopyFrom = nullptr)
{
	checkVoxelSlow(Struct->IsChildOf(StaticStructFast<T>()));

	void* Memory = FMemory::Malloc(FMath::Max(1, Struct->GetStructureSize()));
	Struct->InitializeStruct(Memory);

	if (StructToCopyFrom)
	{
		Struct->CopyScriptStruct(Memory, StructToCopyFrom);
	}

	return TUniquePtr<T>(static_cast<T*>(Memory));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE T* ResolveObjectPtrFast(const TObjectPtr<T>& ObjectPtr)
{
	// Don't broadcast a delegate on every access

	if (IsObjectHandleResolved(ObjectPtr.GetHandle()))
	{
		return static_cast<T*>(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(ObjectPtr.GetHandle()));
	}

	return SlowPath_ResolveObjectPtrFast(ObjectPtr);
}
template<typename T>
FORCENOINLINE T* SlowPath_ResolveObjectPtrFast(const TObjectPtr<T>& ObjectPtr)
{
	return ObjectPtr.Get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE TWeakInterfacePtr<T> MakeWeakInterfacePtr(T* Ptr)
{
	return TWeakInterfacePtr<T>(Ptr);
}
template<typename T, typename OtherType>
FORCEINLINE TWeakInterfacePtr<T> MakeWeakInterfacePtr(OtherType* Ptr)
{
	return TWeakInterfacePtr<T>(Ptr);
}
template<typename T, typename OtherType>
FORCEINLINE TWeakInterfacePtr<T> MakeWeakInterfacePtr(const TObjectPtr<OtherType> Ptr)
{
	return TWeakInterfacePtr<T>(Ptr.Get());
}
template<typename T>
FORCEINLINE TWeakInterfacePtr<T> MakeWeakInterfacePtr(const TScriptInterface<T>& Ptr)
{
	return TWeakInterfacePtr<T>(Ptr.GetInterface());
}

template<typename T>
FORCEINLINE uint32 GetTypeHash(const TWeakInterfacePtr<T>& Ptr)
{
	return GetTypeHash(Ptr.GetWeakObjectPtr());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE FText GetEnumDisplayName(const T Value)
{
	static UEnum* Enum = StaticEnumFast<T>();
	return Enum->GetDisplayNameTextByValue(int64(Value));
}