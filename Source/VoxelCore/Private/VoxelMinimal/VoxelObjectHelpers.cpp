// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

UScriptStruct* FindCoreStruct(const TCHAR* Name)
{
	VOXEL_FUNCTION_COUNTER();

	static UPackage* Package = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	return FindObjectChecked<UScriptStruct>(Package, Name);
}

void ForEachAssetDataOfClass(
	const UClass* ClassToLookFor,
	const TFunctionRef<void(const FAssetData&)> Operation)
{
	VOXEL_FUNCTION_COUNTER();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	if (!FPlatformProperties::RequiresCookedData() &&
		!AssetRegistry.IsSearchAllAssets())
	{
		// Force search all assets in standalone
		AssetRegistry.SearchAllAssets(true);
	}

	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.WaitForCompletion();
		ensure(!AssetRegistry.IsLoadingAssets());
	}

	TVoxelArray<FAssetData> AssetDatas;

	FARFilter Filter;
	Filter.ClassPaths.Add(ClassToLookFor->GetClassPathName());
	Filter.bRecursiveClasses = true;
	AssetRegistry.GetAssets(Filter, AssetDatas);

	for (const FAssetData& AssetData : AssetDatas)
	{
		Operation(AssetData);
	}
}

void ForEachAssetOfClass(
	const UClass* ClassToLookFor,
	const TFunctionRef<void(UObject*)> Operation)
{
	ForEachAssetDataOfClass(ClassToLookFor, [&](const FAssetData& AssetData)
	{
		UObject* Asset = AssetData.GetAsset();
		if (!ensure(Asset) ||
			!ensure(Asset->IsA(ClassToLookFor)))
		{
			return;
		}

		Operation(Asset);
	});
}

TVoxelArray<UScriptStruct*> GetDerivedStructs(const UScriptStruct* BaseStruct, const bool bIncludeBase)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<UScriptStruct*> Result;
	ForEachObjectOfClass<UScriptStruct>([&](UScriptStruct& Struct)
	{
		if (!Struct.IsChildOf(BaseStruct))
		{
			return;
		}
		if (!bIncludeBase &&
			BaseStruct == &Struct)
		{
			return;
		}

		Result.Add(&Struct);
	});
	Result.Shrink();
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool IsFunctionInput(const FProperty& Property)
{
	ensure(Property.Owner.IsA(UFunction::StaticClass()));

	return
		!Property.HasAnyPropertyFlags(CPF_ReturnParm) &&
		(!Property.HasAnyPropertyFlags(CPF_OutParm) || Property.HasAnyPropertyFlags(CPF_ReferenceParm));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FRestoreClassInfo
{
public:
	static const TMap<FName, TObjectPtr<UFunction>>& GetFunctionMap(const UClass* Class)
	{
		return ReinterpretCastRef<TMap<FName, TObjectPtr<UFunction>>>(Class->FuncMap);
	}
};

TVoxelArray<UFunction*> GetClassFunctions(const UClass* Class, const bool bIncludeSuper)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<UFunction*> Functions;
	{
		const TMap<FName, TObjectPtr<UFunction>>& Map = FRestoreClassInfo::GetFunctionMap(Class);

		Functions.Reserve(bIncludeSuper ? FMath::Max(Map.Num(), 128) : Map.Num());

		for (const auto& It : Map)
		{
			Functions.Add(It.Value);
		}
	}

	if (bIncludeSuper)
	{
		const UClass* SuperClass = Class->GetSuperClass();
		while (SuperClass)
		{
			for (auto& It : FRestoreClassInfo::GetFunctionMap(SuperClass))
			{
				Functions.Add(It.Value);
			}
			SuperClass = Class->GetSuperClass();
		}
	}

	return Functions;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FString GetStringMetaDataHierarchical(const UStruct* Struct, const FName Name)
{
	FString Result;
	Struct->GetStringMetaDataHierarchical(Name, &Result);
	return Result;
}
#endif

void ForeachObjectReference(
	UObject& Object,
	const TFunctionRef<void(UObject*& ObjectRef)> Lambda,
	const EPropertyFlags SkipFlags)
{
	VOXEL_FUNCTION_COUNTER();

	class FArchiveForeachObjectReference : public FArchiveUObject
	{
	public:
		const TFunctionRef<void(UObject*&)> Lambda;
		const EPropertyFlags SkipFlags;

		explicit FArchiveForeachObjectReference(
			const TFunctionRef<void(UObject*&)> Lambda,
			const EPropertyFlags SkipFlags)
			: Lambda(Lambda)
			, SkipFlags(SkipFlags)
		{
			ArIsObjectReferenceCollector = true;
			ArIsModifyingWeakAndStrongReferences = true;
		}

		//~ Begin FArchiveUObject Interface
		virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
		{
			return InProperty->HasAnyPropertyFlags(SkipFlags);
		}

		virtual FArchive& operator<<(UObject*& Object) override
		{
			Lambda(Object);
			return *this;
		}
		virtual FArchive& operator<<(FSoftObjectPtr& Value) override
		{
			UObject* Object = Value.LoadSynchronous();
			*this << Object;
			Value = Object;

			return *this;
		}
		//~ End FArchiveUObject Interface
	};

	FArchiveForeachObjectReference Archive(Lambda, SkipFlags);
	Object.Serialize(Archive);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct& Struct, const EFieldIterationFlags IterationFlags)
{
	return TFieldRange<FProperty>(&Struct, IterationFlags);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct* Struct, const EFieldIterationFlags IterationFlags)
{
	check(Struct);
	return TFieldRange<FProperty>(Struct, IterationFlags);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass& Class, const EFieldIterationFlags IterationFlags)
{
	return TFieldRange<FProperty>(&Class, IterationFlags);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass* Class, const EFieldIterationFlags IterationFlags)
{
	check(Class);
	return TFieldRange<FProperty>(Class, IterationFlags);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction& Function, const EFieldIterationFlags IterationFlags)
{
	return TFieldRange<FProperty>(&Function, IterationFlags);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction* Function, const EFieldIterationFlags IterationFlags)
{
	check(Function);
	return TFieldRange<FProperty>(Function, IterationFlags);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSharedVoidRef MakeSharedStruct(const UScriptStruct* Struct, const void* StructToCopyFrom)
{
	checkVoxelSlow(Struct);
	checkVoxelSlow(UObjectInitialized());

	void* Memory = FMemory::Malloc(FMath::Max(1, Struct->GetStructureSize()));
	Struct->InitializeStruct(Memory);

	if (StructToCopyFrom)
	{
		Struct->CopyScriptStruct(Memory, StructToCopyFrom);
	}

	return MakeShareableStruct(Struct, Memory);
}

FSharedVoidRef MakeShareableStruct(const UScriptStruct* Struct, void* StructMemory)
{
	if (Struct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
	{
		const TSharedRef<FVoxelVirtualStruct> SharedRef = MakeShareable(static_cast<FVoxelVirtualStruct*>(StructMemory));
		SharedRef->Internal_UpdateWeakReferenceInternal(SharedRef);
		return MakeSharedVoidRef(SharedRef);
	}

	return MakeSharedVoidRef(TSharedPtr<int32>(static_cast<int32*>(StructMemory), [Struct](void* InMemory)
	{
		FVoxelUtilities::DestroyStruct_Safe(Struct, InMemory);
		FMemory::Free(InMemory);
	}).ToSharedRef());
}

FName GetObjectKeyName(const FObjectKey ObjectKey)
{
	checkVoxelSlow(IsInGameThread());

	const UObject* Object = ObjectKey.ResolveObjectPtr();
	if (!Object)
	{
		return STATIC_FNAME("<null>");
	}

	return Object->GetFName();
}