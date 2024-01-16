// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/UObjectHash.h"
#include "UObject/WeakInterfacePtr.h"
#include "Serialization/BulkData.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"

class FUObjectToken;

template<typename>
class TVoxelStructView;
using FVoxelStructView = TVoxelStructView<void>;

extern VOXELCORE_API bool GVoxelDoNotCreateSubobjects;
#if WITH_EDITOR
extern VOXELCORE_API TArray<TFunction<bool(const UObject&)>> GVoxelTryFocusObjectFunctions;
#endif

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
FORCEINLINE To* CastEnsured(const TObjectPtr<From>& Src)
{
	return CastEnsured<To>(Src.Get());
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
		StaticValue = TBaseStructure<std::decay_t<Struct>>::Get();
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

template<typename ToType, typename FromType, typename Allocator, typename = typename TEnableIf<TIsDerivedFrom<
	std::remove_const_t<ToType>,
	std::remove_const_t<FromType>
>::Value>::Type>
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

template<typename T>
FORCEINLINE TWeakObjectPtr<T> MakeWeakObjectPtr(T& Object)
{
	return TWeakObjectPtr<T>(&Object);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
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

template<typename T>
struct TSubclassOfType;

template<typename T>
struct TSubclassOfType<TSubclassOf<T>>
{
	using Type = T;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelObjectUtilities
{
#if WITH_EDITOR
	static FString GetClassDisplayName_EditorOnly(const UClass* Class);
	static FString GetPropertyTooltip(const UFunction& Function, const FProperty& Property);
	static FString GetPropertyTooltip(const FString& FunctionTooltip, const FString& PropertyName, bool bIsReturnPin);

	static TMap<FName, FString> GetMetadata(const UObject* Object);

	static FString SanitizeCategory(const FString& Category);
	// Will never be empty
	static TArray<FString> ParseCategory(const FString& Category);
	static FString MakeCategory(const TArray<FString>& Categories);
	static bool IsSubCategory(const FString& Category, const FString& SubCategory);

	// Will let the user rename the asset in the content browser
	static void CreateNewAsset_Deferred(UClass* Class, const FString& BaseName, const FString& Suffix, TFunction<void(UObject*)> SetupObject);

	template<typename T>
	static void CreateNewAsset_Deferred(const FString& BaseName, const FString& Suffix, TFunction<void(T*)> SetupObject)
	{
		FVoxelObjectUtilities::CreateNewAsset_Deferred(T::StaticClass(), BaseName, Suffix, [=](UObject* Object) { SetupObject(CastChecked<T>(Object)); });
	}

	static UObject* CreateNewAsset_Direct(UClass* Class, const FString& BaseName, const FString& Suffix);

	template<typename T>
	static T* CreateNewAsset_Direct(const FString& BaseName, const FString& Suffix)
	{
		return CastChecked<T>(FVoxelObjectUtilities::CreateNewAsset_Direct(T::StaticClass(), BaseName, Suffix), ECastCheckedType::NullAllowed);
	}
	template<typename T>
	static T* CreateNewAsset_Direct(const UObject* ObjectWithPath, const FString& Suffix)
	{
		return CreateNewAsset_Direct<T>(FPackageName::ObjectPathToPackageName(ObjectWithPath->GetPathName()), Suffix);
	}
#endif

#if WITH_EDITOR
	static void FocusObject(const UObject* Object);
	static void FocusObject(const UObject& Object);
#endif
	static void InvokeFunctionWithNoParameters(UObject* Object, UFunction* Function);

public:
	static bool PropertyFromText_Direct(const FProperty& Property, const FString& Text, void* Data, UObject* Owner);
	template<typename T>
	static bool PropertyFromText_Direct(const FProperty& Property, const FString& Text, T* Data, UObject* Owner) = delete;

	static bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, void* ContainerData, UObject* Owner);
	template<typename T>
	static bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, T* ContainerData, UObject* Owner) = delete;

	static bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, UObject* Owner);

	template<typename T>
	static bool TryImportText(const FString& Text, T& OutValue)
	{
		return PropertyFromText_Direct(
			*MakeStructProperty<T>(),
			*Text,
			reinterpret_cast<void*>(&OutValue),
			nullptr);
	}

public:
	static FString PropertyToText_Direct(const FProperty& Property, const void* Data, const UObject* Owner);
	template<typename T>
	static FString PropertyToText_Direct(const FProperty& Property, const T* Data, const UObject* Owner) = delete;

	static FString PropertyToText_InContainer(const FProperty& Property, const void* ContainerData, const UObject* Owner);
	template<typename T>
	static FString PropertyToText_InContainer(const FProperty& Property, const T* ContainerData, const UObject* Owner) = delete;

	static FString PropertyToText_InContainer(const FProperty& Property, const UObject* Owner);

public:
	static bool ShouldSerializeBulkData(FArchive& Ar);

	GENERATE_VOXEL_MEMBER_FUNCTION_CHECK(Serialize);

	static void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		TFunctionRef<void()> SaveBulkData,
		TFunctionRef<void()> LoadBulkData);

	static void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		TFunctionRef<void(FArchive&)> Serialize);

	template<typename T>
	static void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		T& Data)
	{
		FVoxelObjectUtilities::SerializeBulkData(Object, BulkData, Ar, [&](FArchive& BulkDataAr)
		{
			if constexpr (THasMemberFunction_Serialize<T>::Value)
			{
				Data.Serialize(BulkDataAr);
			}
			else
			{
				BulkDataAr << Data;
			}
		});
	}

public:
	static uint32 HashProperty(const FProperty& Property, const void* DataPtr);
	static void DestroyStruct_Safe(const UScriptStruct* Struct, void* StructMemory);
	static void AddStructReferencedObjects(FReferenceCollector& Collector, const FVoxelStructView& StructView);

	template<typename T>
	static bool AreStructsIdentical(const T& A, const T& B)
	{
		return T::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None);
	}

	// Force load a deprecated object even if Export.bIsInheritedInstance is true
	// Should only be called inside PostCDOContruct
	template<typename T>
	static void ForceLoadDeprecatedSubobject(UObject* Object, const FName Name)
	{
		ensure(!GVoxelDoNotCreateSubobjects);
		GVoxelDoNotCreateSubobjects = true;
		{
			NewObject<T>(Object, Name);
		}
		ensure(GVoxelDoNotCreateSubobjects);
		GVoxelDoNotCreateSubobjects = false;
	}

public:
	static TUniquePtr<FBoolProperty> MakeBoolProperty();
	static TUniquePtr<FFloatProperty> MakeFloatProperty();
	static TUniquePtr<FIntProperty> MakeIntProperty();
	static TUniquePtr<FNameProperty> MakeNameProperty();

	static TUniquePtr<FEnumProperty> MakeEnumProperty(const UEnum* Enum);
	static TUniquePtr<FStructProperty> MakeStructProperty(const UScriptStruct* Struct);
	static TUniquePtr<FObjectProperty> MakeObjectProperty(const UClass* Class);
	static TUniquePtr<FArrayProperty> MakeArrayProperty(FProperty* InnerProperty);

	template<typename T>
	static TUniquePtr<FStructProperty> MakeEnumProperty()
	{
		return FVoxelObjectUtilities::MakeEnumProperty(StaticEnumFast<T>());
	}
	template<typename T>
	static TUniquePtr<FStructProperty> MakeStructProperty()
	{
		return FVoxelObjectUtilities::MakeStructProperty(StaticStructFast<T>());
	}
	template<typename T>
	static TUniquePtr<FStructProperty> MakeObjectProperty()
	{
		return FVoxelObjectUtilities::MakeObjectProperty(StaticClassFast<T>());
	}

public:
	template<typename T>
	using TPropertyType = typename TChooseClass<
		std::is_same_v<T, bool>          , FBoolProperty      , typename TChooseClass<
		std::is_same_v<T, uint8>         , FByteProperty      , typename TChooseClass<
		std::is_same_v<T, float>         , FFloatProperty     , typename TChooseClass<
		std::is_same_v<T, double>        , FDoubleProperty    , typename TChooseClass<
		std::is_same_v<T, int32>         , FIntProperty       , typename TChooseClass<
		std::is_same_v<T, int64>         , FInt64Property     , typename TChooseClass<
		std::is_same_v<T, FName>         , FNameProperty      , typename TChooseClass<
		TIsEnum<T>::Value                , FEnumProperty      , typename TChooseClass<
		TIsDerivedFrom<T, UObject>::Value, FObjectProperty    , typename TChooseClass<
		TIsTObjectPtr<T>::Value          , FObjectPtrProperty , typename TChooseClass<
		TIsSoftObjectPtr<T>::Value       , FSoftObjectProperty, typename TChooseClass<
		TIsTSubclassOf<T>::Value         , FClassProperty
	                                     , FStructProperty
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result
	>::Result;

	template<typename T>
	static bool MatchesProperty(const FProperty& Property, const bool bAllowInheritance)
	{
		return FVoxelObjectUtilities::MatchesPropertyImpl(Property, *reinterpret_cast<T*>(-1), bAllowInheritance);
	}

private:
	template<typename T>
	static bool MatchesPropertyImpl(const FProperty& Property, T&, const bool bAllowInheritance)
	{
		using PropertyType = TPropertyType<T>;

		if (!Property.IsA<PropertyType>())
		{
			return false;
		}

		const auto Matches = [&](const UStruct* Base, const UStruct* Target)
		{
			if (bAllowInheritance)
			{
				return Base->IsChildOf(Target);
			}
			return Base == Target;
		};

		if constexpr (std::is_same_v<PropertyType, FEnumProperty>)
		{
			return CastFieldChecked<FEnumProperty>(Property).GetEnum() == StaticEnumFast<T>();
		}
		else if constexpr (std::is_same_v<PropertyType, FStructProperty>)
		{
			return Matches(CastFieldChecked<FStructProperty>(Property).Struct, StaticStructFast<T>());
		}
		else if constexpr (std::is_same_v<PropertyType, FObjectProperty>)
		{
			return Matches(CastFieldChecked<FObjectProperty>(Property).PropertyClass, StaticClassFast<T>());
		}
		else if constexpr (std::is_same_v<PropertyType, FObjectPtrProperty>)
		{
			return Matches(CastFieldChecked<FObjectProperty>(Property).PropertyClass, StaticClassFast<typename TRemoveObjectPointer<T>::Type>());
		}
		else if constexpr (std::is_same_v<PropertyType, FSoftObjectProperty>)
		{
			return Matches(CastFieldChecked<FSoftObjectProperty>(Property).PropertyClass, StaticClassFast<T>());
		}
		else
		{
			return true;
		}
	}
	template<typename T>
	static bool MatchesPropertyImpl(const FProperty& Property, T*&, const bool bAllowInheritance)
	{
		checkStatic(TIsDerivedFrom<T, UObject>::Value);
		return MatchesProperty<T>(Property, bAllowInheritance);
	}
	template<typename T, typename AllocatorType>
	static bool MatchesPropertyImpl(const FProperty& Property, TArray<T, AllocatorType>&, const bool bAllowInheritance)
	{
		return
			Property.IsA<FArrayProperty>() &&
			MatchesProperty<T>(*CastFieldChecked<FArrayProperty>(Property).Inner, bAllowInheritance);
	}
	template<typename T>
	static bool MatchesPropertyImpl(const FProperty& Property, TSet<T>&, const bool bAllowInheritance)
	{
		return
			Property.IsA<FSetProperty>() &&
			MatchesProperty<T>(*CastFieldChecked<FSetProperty>(Property).ElementProp, bAllowInheritance);
	}
	template<typename Key, typename Value>
	static bool MatchesPropertyImpl(const FProperty& Property, TMap<Key, Value>&, const bool bAllowInheritance)
	{
		return
			Property.IsA<FMapProperty>() &&
			MatchesProperty<Key>(*CastFieldChecked<FMapProperty>(Property).KeyProp, bAllowInheritance) &&
			MatchesProperty<Value>(*CastFieldChecked<FMapProperty>(Property).ValueProp, bAllowInheritance);
	}
};

VOXELCORE_API void ForEachAssetOfClass(
	const UClass* ClassToLookFor,
	TFunctionRef<void(UObject*)> Operation);

template<typename T, typename LambdaType, typename = typename TEnableIf<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>::Type>
void ForEachAssetOfClass(LambdaType&& Operation)
{
	ForEachAssetOfClass(T::StaticClass(), [&](UObject* Asset)
	{
		Operation(*CastChecked<T>(Asset));
	});
}

template<typename T, typename LambdaType, typename = typename TEnableIf<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>::Type>
void ForEachObjectOfClass(LambdaType&& Operation, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None)
{
	ForEachObjectOfClass(T::StaticClass(), [&](UObject* Object)
	{
		checkVoxelSlow(Object && Object->IsA<T>());
		Operation(*static_cast<T*>(Object));
	}, bIncludeDerivedClasses, ExcludeFlags, ExclusionInternalFlags);
}

template<typename T, typename LambdaType, typename = typename TEnableIf<std::is_void_v<decltype(DeclVal<LambdaType>()(DeclVal<T&>()))>>::Type>
void ForEachObjectOfClass_Copy(LambdaType&& Operation, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None)
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

template<typename T = void, typename ArrayType = typename TChooseClass<std::is_same_v<T, void>, UClass*, TSubclassOf<T>>::Result>
TArray<ArrayType> GetDerivedClasses(const UClass* BaseClass = T::StaticClass(), const bool bRecursive = true, const bool bRemoveDeprecated = true)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<UClass*> Result;
	GetDerivedClasses(BaseClass, Result, bRecursive);

	if (bRemoveDeprecated)
	{
		Result.RemoveAllSwap([](const UClass* Class)
		{
			return Class->HasAnyClassFlags(CLASS_Deprecated);
		});
	}

	return ReinterpretCastArray<ArrayType>(MoveTemp(Result));
}

VOXELCORE_API TArray<UScriptStruct*> GetDerivedStructs(const UScriptStruct* BaseStruct, bool bIncludeBase = false);

template<typename T, bool bIncludeBase = false>
TArray<UScriptStruct*> GetDerivedStructs()
{
	return GetDerivedStructs(T::StaticStruct(), bIncludeBase);
}

VOXELCORE_API bool IsFunctionInput(const FProperty& Property);
VOXELCORE_API TArray<UFunction*> GetClassFunctions(const UClass* Class, bool bIncludeSuper = false);

#if WITH_EDITOR
VOXELCORE_API FString GetStringMetaDataHierarchical(const UStruct* Struct, FName Name);
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelNullCheckReferenceCollector : public FReferenceCollector
{
public:
	FReferenceCollector& ReferenceCollector;

	explicit FVoxelNullCheckReferenceCollector(FReferenceCollector& ReferenceCollector)
		: ReferenceCollector(ReferenceCollector)
	{
	}

	virtual void AddStableReference(UObject** Object) override;
	virtual void AddStableReferenceArray(TArray<UObject*>* Objects) override;
	virtual void AddStableReferenceSet(TSet<UObject*>* Objects) override;
	virtual bool NeedsPropertyReferencer() const override;
	virtual bool IsIgnoringArchetypeRef() const override;
	virtual bool IsIgnoringTransient() const override;
	virtual void AllowEliminatingReferences(bool bAllow) override;
	virtual void SetSerializedProperty(FProperty* InProperty) override;
	virtual FProperty* GetSerializedProperty() const override;
	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference) override;
	virtual void SetIsProcessingNativeReferences(bool bIsNative) override;
	virtual bool IsProcessingNativeReferences() const override;
	virtual bool NeedsInitialReferences() const override;
	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename PropertyType>
struct TVoxelPropertiesIterator
{
public:
	TVoxelPropertiesIterator() = default;
	FORCEINLINE explicit TVoxelPropertiesIterator(const UStruct& Struct)
		: Iterator(&Struct)
	{
		FindNextProperty();
	}

	FORCEINLINE FProperty& operator*() const
	{
		FProperty& Property = ConstCast(**Iterator);

		if constexpr (!std::is_same_v<PropertyType, void>)
		{
			checkVoxelSlow(FVoxelObjectUtilities::MatchesProperty<PropertyType>(Property, true));
		}

		return Property;
	}
	FORCEINLINE TVoxelPropertiesIterator& operator++()
	{
		++Iterator;
		FindNextProperty();
		return *this;
	}

	FORCEINLINE bool operator!=(decltype(nullptr)) const
	{
		return bool(Iterator);
	}

private:
	TFieldIterator<FProperty> Iterator;

	FORCEINLINE void FindNextProperty()
	{
		if constexpr (!std::is_same_v<PropertyType, void>)
		{
			while (bool(Iterator) && !FVoxelObjectUtilities::MatchesProperty<PropertyType>(**Iterator, true))
			{
				++Iterator;
			}
		}
	}
};

template<typename PropertyType>
struct TVoxelStructRangedFor
{
	const UStruct& Struct;

	FORCEINLINE TVoxelStructRangedFor(const UStruct& Struct)
		: Struct(Struct)
	{
	}

	FORCEINLINE FProperty* First() const
	{
		return Struct.PropertyLink;
	}

	FORCEINLINE TVoxelPropertiesIterator<PropertyType> begin() const
	{
		return TVoxelPropertiesIterator<PropertyType>(Struct);
	}
	FORCEINLINE decltype(nullptr) end() const
	{
		return nullptr;
	}

	int32 Num() const
	{
		int32 Result = 0;
		for (FProperty& Property : *this)
		{
			Result++;
		}
		return Result;
	}
	TVoxelArray<FProperty*> Array() const
	{
		TVoxelArray<FProperty*> Result;
		for (FProperty& Property : *this)
		{
			Result.Add(&Property);
		}
		return Result;
	}
};

template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetStructProperties(const UStruct& Struct)
{
	return TVoxelStructRangedFor<PropertyType>(Struct);
}
template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetStructProperties(const UStruct* Struct)
{
	check(Struct);
	return TVoxelStructRangedFor<PropertyType>(*Struct);
}
template<typename T, typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetStructProperties()
{
	return GetStructProperties<PropertyType>(StaticStructFast<T>());
}

template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetClassProperties(const UClass& Class)
{
	return TVoxelStructRangedFor<PropertyType>(Class);
}
template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetClassProperties(const UClass* Class)
{
	check(Class);
	return TVoxelStructRangedFor<PropertyType>(*Class);
}
template<typename T, typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetClassProperties()
{
	return GetClassProperties<PropertyType>(StaticClassFast<T>());
}

template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetFunctionProperties(const UFunction& Function)
{
	return TVoxelStructRangedFor<PropertyType>(Function);
}
template<typename PropertyType = void>
FORCEINLINE TVoxelStructRangedFor<PropertyType> GetFunctionProperties(const UFunction* Function)
{
	check(Function);
	return TVoxelStructRangedFor<PropertyType>(*Function);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelPropertyValueIterator
{
public:
	TVoxelPropertyValueIterator() = default;
	FORCEINLINE TVoxelPropertyValueIterator(const UStruct& Struct, void* StructMemory)
		: Iterator(Struct)
		, StructMemory(StructMemory)
	{
	}

	FORCEINLINE FProperty& Key() const
	{
		return *Iterator;
	}
	FORCEINLINE T& Value() const
	{
		const FProperty& Property = *Iterator;
		checkVoxelSlow(FVoxelObjectUtilities::MatchesProperty<T>(Property, true));
		return *Property.ContainerPtrToValuePtr<T>(StructMemory);
	}

	FORCEINLINE const TVoxelPropertyValueIterator& operator*() const
	{
		checkVoxelSlow(FVoxelObjectUtilities::MatchesProperty<T>(*Iterator, true));
		return *this;
	}
	FORCEINLINE TVoxelPropertyValueIterator& operator++()
	{
		++Iterator;
		return *this;
	}

	FORCEINLINE bool operator!=(decltype(nullptr)) const
	{
		return Iterator != nullptr;
	}

private:
	TVoxelPropertiesIterator<T> Iterator;
	void* StructMemory = nullptr;
};

template<typename T>
struct TVoxelPropertyValueIteratorRangedFor
{
	const UStruct& Struct;
	void* const StructMemory;

	FORCEINLINE TVoxelPropertyValueIteratorRangedFor(const UStruct& Struct, void* StructMemory)
		: Struct(Struct)
		, StructMemory(StructMemory)
	{
	}

	FORCEINLINE const FProperty* First() const
	{
		return Struct.PropertyLink;
	}

	FORCEINLINE TVoxelPropertyValueIterator<T> begin() const
	{
		return TVoxelPropertyValueIterator<T>(Struct, StructMemory);
	}
	FORCEINLINE decltype(nullptr) end() const
	{
		return nullptr;
	}
};

template<typename T>
FORCEINLINE TVoxelPropertyValueIteratorRangedFor<T> CreatePropertyValueIterator(const UStruct* Struct, void* StructMemory)
{
	check(Struct);
	check(StructMemory);
	return TVoxelPropertyValueIteratorRangedFor<T>(*Struct, StructMemory);
}
template<typename T>
FORCEINLINE TVoxelPropertyValueIteratorRangedFor<const T> CreatePropertyValueIterator(const UStruct* Struct, const void* StructMemory)
{
	return CreatePropertyValueIterator<const T>(Struct, ConstCast(StructMemory));
}

template<typename T>
FORCEINLINE TVoxelPropertyValueIteratorRangedFor<T> CreatePropertyValueIterator(UObject* Object)
{
	return CreatePropertyValueIterator<T>(Object->GetClass(), Object);
}
template<typename T>
FORCEINLINE TVoxelPropertyValueIteratorRangedFor<const T> CreatePropertyValueIterator(const UObject* Object)
{
	return CreatePropertyValueIterator<T>(Object->GetClass(), Object);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FProperty& FindFPropertyChecked_Impl(const FName Name)
{
	UStruct* Struct;
	if constexpr (TIsDerivedFrom<T, UObject>::Value)
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

struct FVoxelSharedStructOpaque;

VOXELCORE_API TSharedRef<FVoxelSharedStructOpaque> MakeSharedStruct(const UScriptStruct* Struct, const void* StructToCopyFrom = nullptr);
VOXELCORE_API TSharedRef<FVoxelSharedStructOpaque> MakeShareableStruct(const UScriptStruct* Struct, void* StructMemory);

template<typename T>
TSharedRef<T> MakeSharedStruct(const UScriptStruct* Struct, const T* StructToCopyFrom = nullptr)
{
	check(Struct->IsChildOf(StaticStructFast<T>()));

	const TSharedRef<T> SharedRef = ReinterpretCastRef<TSharedRef<T>>(MakeSharedStruct(Struct, static_cast<const void*>(StructToCopyFrom)));
	SharedPointerInternals::EnableSharedFromThis(&SharedRef, &SharedRef.Get(), &SharedRef.Get());
	return SharedRef;
}

template<typename T>
FORCEINLINE FObjectKey MakeObjectKey(const TWeakObjectPtr<T>& Ptr)
{
	return ReinterpretCastRef<FObjectKey>(Ptr);
}

template<typename T>
FORCEINLINE T* ResolveObjectPtrFast(const TObjectPtr<T>& ObjectPtr)
{
	if (IsObjectHandleResolved(ObjectPtr.GetHandle()))
	{
		return static_cast<T*>(UE::CoreUObject::Private::ReadObjectHandlePointerNoCheck(ObjectPtr.GetHandle()));
	}

	return SlowPath_ResolveObjectPtrFast(ObjectPtr);
}
template<typename T>
FORCEINLINE T* SlowPath_ResolveObjectPtrFast(const TObjectPtr<T>& ObjectPtr)
{
	return ObjectPtr.Get();
}

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