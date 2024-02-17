// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

void ForEachAssetOfClass(const UClass* ClassToLookFor, TFunctionRef<void(UObject*)> Operation)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDatas;

	FARFilter Filter;
	Filter.ClassPaths.Add(ClassToLookFor->GetClassPathName());
	AssetRegistryModule.Get().GetAssets(Filter, AssetDatas);

	for (const FAssetData& AssetData : AssetDatas)
	{
		UObject* Asset = AssetData.GetAsset();
		if (!ensure(Asset) ||
			!ensure(Asset->IsA(ClassToLookFor)))
		{
			continue;
		}

		Operation(Asset);
	}
}

TArray<UScriptStruct*> GetDerivedStructs(const UScriptStruct* BaseStruct, const bool bIncludeBase)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<UScriptStruct*> Result;
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

TArray<UFunction*> GetClassFunctions(const UClass* Class, const bool bIncludeSuper)
{
	TArray<TObjectPtr<UFunction>> Functions;
	FRestoreClassInfo::GetFunctionMap(Class).GenerateValueArray(Functions);

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct& Struct)
{
	return TFieldRange<FProperty>(&Struct);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetStructProperties(const UStruct* Struct)
{
	check(Struct);
	return TFieldRange<FProperty>(Struct);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass& Class)
{
	return TFieldRange<FProperty>(&Class);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetClassProperties(const UClass* Class)
{
	check(Class);
	return TFieldRange<FProperty>(Class);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction& Function)
{
	return TFieldRange<FProperty>(&Function);
}

TVoxelDereferencingRange<TFieldRange<FProperty>> GetFunctionProperties(const UFunction* Function)
{
	check(Function);
	return TFieldRange<FProperty>(Function);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSharedVoidRef MakeSharedStruct(const UScriptStruct* Struct, const void* StructToCopyFrom)
{
	check(Struct);
	checkVoxelSlow(UObjectInitialized());

	void* Memory = FVoxelMemory::Malloc(FMath::Max(1, Struct->GetStructureSize()));
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
		const TSharedRef<FVoxelVirtualStruct> SharedRef = MakeVoxelShareable(static_cast<FVoxelVirtualStruct*>(StructMemory));
		SharedRef->Internal_UpdateWeakReferenceInternal(SharedRef);
		return MakeSharedVoidRef(SharedRef);
	}

	return MakeSharedVoidRef(TSharedPtr<int32>(static_cast<int32*>(StructMemory), [Struct](void* InMemory)
	{
		FVoxelUtilities::DestroyStruct_Safe(Struct, InMemory);
		FVoxelMemory::Free(InMemory);
	}).ToSharedRef());
}