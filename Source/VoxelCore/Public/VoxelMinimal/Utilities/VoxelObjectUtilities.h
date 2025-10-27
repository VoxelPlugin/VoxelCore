// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Serialization/BulkData.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/VoxelStructView.h"
#include "VoxelMinimal/VoxelObjectHelpers.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

class FUObjectToken;

extern VOXELCORE_API bool GVoxelDoNotCreateSubobjects;
#if WITH_EDITOR
extern VOXELCORE_API TVoxelArray<TVoxelUniqueFunction<bool(const UObject&)>> GVoxelTryFocusObjectFunctions;
#endif
extern VOXELCORE_API TVoxelArray<TVoxelUniqueFunction<FString(const UObject&)>> GVoxelTryGetObjectNameFunctions;

VOXELCORE_API void SerializeVoxelVersion(FArchive& Ar);

namespace FVoxelUtilities
{
#if WITH_EDITOR
	VOXELCORE_API FString GetClassDisplayName_EditorOnly(const UClass* Class);
	VOXELCORE_API FString GetPropertyTooltip(const UFunction& Function, const FProperty& Property);
	VOXELCORE_API FString GetPropertyTooltip(const FString& FunctionTooltip, const FString& PropertyName, bool bIsReturnPin);
	VOXELCORE_API FString GetStructTooltip(const UStruct& Struct);
	VOXELCORE_API FString ParseStructTooltip(FString Tooltip);

	VOXELCORE_API FString GetFunctionType(const FProperty& Property);
	VOXELCORE_API TMap<FName, FString> GetMetadata(const UObject* Object);

	VOXELCORE_API FString SanitizeCategory(const FString& Category);
	// Will never be empty
	VOXELCORE_API TArray<FString> ParseCategory(const FString& Category);
	VOXELCORE_API FString MakeCategory(const TArray<FString>& Categories);
	VOXELCORE_API bool IsSubCategory(const FString& Category, const FString& SubCategory);
#endif

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	// Will let the user rename the asset in the content browser
	VOXELCORE_API void CreateNewAsset_Deferred(
		UClass* Class,
		const FString& BaseName,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix,
		TFunction<void(UObject*)> SetupObject);
	VOXELCORE_API UObject* CreateNewAsset_Direct(
		UClass* Class,
		const FString& BaseName,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix);

	template<typename T>
	void CreateNewAsset_Deferred(
		const FString& BaseName,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix,
		TFunction<void(T*)> SetupObject)
	{
		FVoxelUtilities::CreateNewAsset_Deferred(
			T::StaticClass(),
			BaseName,
			PrefixesToRemove,
			NewPrefix,
			Suffix,
			[=](UObject* Object) { SetupObject(CastChecked<T>(Object)); });
	}
	template<typename T>
	T* CreateNewAsset_Direct(
		const FString& BaseName,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix)
	{
		return CastChecked<T>(FVoxelUtilities::CreateNewAsset_Direct(
			T::StaticClass(),
			BaseName,
			PrefixesToRemove,
			NewPrefix,
			Suffix), ECastCheckedType::NullAllowed);
	}
	template<typename T>
	T* CreateNewAsset_Direct(
		const UObject* ObjectWithPath,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix)
	{
		return CreateNewAsset_Direct<T>(
			FPackageName::ObjectPathToPackageName(ObjectWithPath->GetPathName()),
			PrefixesToRemove,
			NewPrefix,
			Suffix);
	}

	VOXELCORE_API void CreateUniqueAssetName(
		const FString& BasePackageName,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix,
		FString& OutPackageName,
		FString& OutAssetName);
	VOXELCORE_API void CreateUniqueAssetName(
		const UObject* ObjectWithPath,
		const TArray<FString>& PrefixesToRemove,
		const FString& NewPrefix,
		const FString& Suffix,
		FString& OutPackageName,
		FString& OutAssetName);
#endif

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	VOXELCORE_API void FocusObject(const UObject* Object);
	VOXELCORE_API void FocusObject(const UObject& Object);
#endif

	VOXELCORE_API FString GetReadableName(const UObject* Object);
	VOXELCORE_API FString GetReadableName(const UObject& Object);

	VOXELCORE_API void InvokeFunctionWithNoParameters(UObject* Object, UFunction* Function);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FString GetArchivePath(const FArchive& Ar);
	VOXELCORE_API bool ShouldSerializeBulkData(FArchive& Ar);

	VOXELCORE_API void SerializeStruct(
		FArchive& Ar,
		UScriptStruct*& Struct);

	GENERATE_VOXEL_MEMBER_FUNCTION_CHECK(Serialize);

	VOXELCORE_API void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		TFunctionRef<void()> SaveBulkData,
		TFunctionRef<void()> LoadBulkData);

	VOXELCORE_API void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		TFunctionRef<void(FArchive&)> Serialize);

	template<typename T>
	void SerializeBulkData(
		UObject* Object,
		FByteBulkData& BulkData,
		FArchive& Ar,
		T& Data)
	{
		FVoxelUtilities::SerializeBulkData(Object, BulkData, Ar, [&](FArchive& BulkDataAr)
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

	VOXELCORE_API TVoxelFuture<TSharedPtr<TVoxelArray64<uint8>>> ReadBulkDataAsync(
		const FBulkData& BulkData,
		int64 Offset,
		int64 Length,
		EAsyncIOPriorityAndFlags Priority = AIOP_Normal);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API UObject* NewObject_Safe(UObject* Outer, const UClass* Class, FName Name);

	template<typename Type>
	Type* NewObject_Safe(UObject* Outer, const FName Name)
	{
		return CastChecked<Type>(FVoxelUtilities::NewObject_Safe(Outer, Type::StaticClass(), Name));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API uint64 HashProperty(const FProperty& Property, const void* DataPtr);
	VOXELCORE_API void DestroyStruct_Safe(const UScriptStruct* Struct, void* StructMemory);
	VOXELCORE_API void AddStructReferencedObjects(FReferenceCollector& Collector, const FVoxelStructView& StructView);

	template<typename T>
	void AddStructReferencedObjects(FReferenceCollector& Collector, T& Struct)
	{
		FVoxelUtilities::AddStructReferencedObjects(Collector, FVoxelStructView::Make(Struct));
	}

	// Force load a deprecated object even if Export.bIsInheritedInstance is true
	// Should only be called inside PostCDOContruct
	template<typename T>
	void ForceLoadDeprecatedSubobject(UObject* Object, const FName Name)
	{
		ensure(!GVoxelDoNotCreateSubobjects);
		GVoxelDoNotCreateSubobjects = true;
		{
			NewObject<T>(Object, Name);
		}
		ensure(GVoxelDoNotCreateSubobjects);
		GVoxelDoNotCreateSubobjects = false;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API const FBoolProperty& MakeBoolProperty();
	VOXELCORE_API const FFloatProperty& MakeFloatProperty();
	VOXELCORE_API const FIntProperty& MakeIntProperty();
	VOXELCORE_API const FNameProperty& MakeNameProperty();

	VOXELCORE_API TUniquePtr<FEnumProperty> MakeEnumProperty(const UEnum* Enum);
	VOXELCORE_API TUniquePtr<FStructProperty> MakeStructProperty(const UScriptStruct* Struct);
	VOXELCORE_API TUniquePtr<FObjectProperty> MakeObjectProperty(const UClass* Class);
	VOXELCORE_API TUniquePtr<FArrayProperty> MakeArrayProperty(FProperty* InnerProperty);

	template<typename T>
	const FEnumProperty& MakeEnumProperty()
	{
		static const TUniquePtr<FEnumProperty>& Property = *FVoxelUtilities::MakeEnumProperty(StaticEnumFast<T>());
		return *Property;
	}
	template<typename T>
	const FStructProperty& MakeStructProperty()
	{
		static const TUniquePtr<FStructProperty>& Property = FVoxelUtilities::MakeStructProperty(StaticStructFast<T>());
		return *Property;
	}
	template<typename T>
	const FObjectProperty& MakeObjectProperty()
	{
		static const TUniquePtr<FObjectProperty>& Property = FVoxelUtilities::MakeObjectProperty(StaticClassFast<T>());
		return *Property;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	using TPropertyType = std::conditional_t<
		std::is_same_v<T, bool>      , FBoolProperty      , std::conditional_t<
		std::is_same_v<T, uint8>     , FByteProperty      , std::conditional_t<
		std::is_same_v<T, float>     , FFloatProperty     , std::conditional_t<
		std::is_same_v<T, double>    , FDoubleProperty    , std::conditional_t<
		std::is_same_v<T, int32>     , FIntProperty       , std::conditional_t<
		std::is_same_v<T, int64>     , FInt64Property     , std::conditional_t<
		std::is_same_v<T, FName>     , FNameProperty      , std::conditional_t<
		std::is_enum_v<T>            , FEnumProperty      , std::conditional_t<
		std::derived_from<T, UObject>, FObjectProperty    , std::conditional_t<
		TIsTObjectPtr<T>::Value      , FObjectProperty    , std::conditional_t<
		TIsSoftObjectPtr<T>::Value   , FSoftObjectProperty, std::conditional_t<
		TIsTSubclassOf<T>::Value     , FClassProperty
			                         , FStructProperty
		>>>>>>>>>>>>;

	template<typename T>
	bool MatchesProperty(const FProperty& Property, bool bAllowInheritance);

	template<typename T, typename ContainerType>
	auto TryGetProperty(ContainerType& Container, const FProperty& Property) -> std::conditional_t<std::is_const_v<ContainerType>, const T*, T*>
	{
		if (!FVoxelUtilities::MatchesProperty<T>(Property, true))
		{
			return nullptr;
		}

		return Property.ContainerPtrToValuePtr<T>(&Container);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API bool PropertyFromText_Direct(const FProperty& Property, const FString& Text, void* Data, UObject* Owner);
	VOXELCORE_API bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, void* ContainerData, UObject* Owner);
	VOXELCORE_API bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, UObject* Owner);

	template<typename T>
	bool PropertyFromText_Direct(const FProperty& Property, const FString& Text, T* Data, UObject* Owner) = delete;
	template<typename T>
	bool PropertyFromText_InContainer(const FProperty& Property, const FString& Text, T* ContainerData, UObject* Owner) = delete;

	template<typename T>
	bool TryImportText(const FString& Text, T& OutValue)
	{
		return PropertyFromText_Direct(
			MakeStructProperty<T>(),
			*Text,
			reinterpret_cast<void*>(&OutValue),
			nullptr);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FString PropertyToText_Direct(const FProperty& Property, const void* Data, const UObject* Owner);
	template<typename T>
	FString PropertyToText_Direct(const FProperty& Property, const T* Data, const UObject* Owner) = delete;

	VOXELCORE_API FString PropertyToText_InContainer(const FProperty& Property, const void* ContainerData, const UObject* Owner);
	template<typename T>
	FString PropertyToText_InContainer(const FProperty& Property, const T* ContainerData, const UObject* Owner) = delete;

	VOXELCORE_API FString PropertyToText_InContainer(const FProperty& Property, const UObject* Owner);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace FVoxelUtilities::Internal
{
	template<typename T>
	bool MatchesPropertyImpl(const FProperty& Property, T&, const bool bAllowInheritance)
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
		checkStatic(std::derived_from<T, UObject>);
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
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
bool FVoxelUtilities::MatchesProperty(const FProperty& Property, const bool bAllowInheritance)
{
	return Internal::MatchesPropertyImpl(Property, *reinterpret_cast<T*>(-1), bAllowInheritance);
}