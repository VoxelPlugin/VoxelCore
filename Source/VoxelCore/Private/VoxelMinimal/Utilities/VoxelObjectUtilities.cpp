// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "EdGraph/EdGraph.h"
#include "UObject/MetaData.h"
#include "Misc/UObjectToken.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

bool GVoxelDoNotCreateSubobjects = false;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
TArray<TFunction<bool(const UObject&)>> GVoxelTryFocusObjectFunctions;

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	GVoxelTryFocusObjectFunctions.Add([](const UObject& Object)
	{
		const UEdGraph* EdGraph = Cast<UEdGraph>(&Object);
		if (!EdGraph)
		{
			return false;
		}

		const TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(EdGraph, true);
		if (!BlueprintEditor.IsValid())
		{
			return false;
		}

		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EdGraph);
		return true;
	});

	GVoxelTryFocusObjectFunctions.Add([](const UObject& Object)
	{
		const UEdGraphNode* EdGraphNode = Cast<UEdGraphNode>(&Object);
		if (!EdGraphNode)
		{
			return false;
		}

		const TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(EdGraphNode, true);
		if (!BlueprintEditor.IsValid())
		{
			return false;
		}

		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EdGraphNode);
		return true;
	});
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FString FVoxelUtilities::GetClassDisplayName_EditorOnly(const UClass* Class)
{
	if (!Class)
	{
		return "NULL";
	}

	FString ClassName = Class->GetDisplayNameText().ToString();

	const TArray<FString> Acronyms = { "RGB", "LOD" };
	for (const FString& Acronym : Acronyms)
	{
		ClassName.ReplaceInline(*Acronym, *(Acronym + " "), ESearchCase::CaseSensitive);
	}

	return ClassName;
}

FString FVoxelUtilities::GetPropertyTooltip(const UFunction& Function, const FProperty& Property)
{
	ensure(Property.Owner == &Function);

	return GetPropertyTooltip(
		Function.GetToolTipText().ToString(),
		Property.GetName(),
		&Property == Function.GetReturnProperty());
}

FString FVoxelUtilities::GetPropertyTooltip(const FString& FunctionTooltip, const FString& PropertyName, const bool bIsReturnPin)
{
	VOXEL_FUNCTION_COUNTER();

	// Same as UK2Node_CallFunction::GeneratePinTooltipFromFunction

	const FString Tag = bIsReturnPin ? "@return" : "@param";

	int32 Position = -1;
	while (Position < FunctionTooltip.Len())
	{
		Position = FunctionTooltip.Find(Tag, ESearchCase::IgnoreCase, ESearchDir::FromStart, Position);
		if (Position == -1) // If the tag wasn't found
		{
			break;
		}

		// Advance past the tag
		Position += Tag.Len();

		// Advance past whitespace
		while (Position < FunctionTooltip.Len() && FChar::IsWhitespace(FunctionTooltip[Position]))
		{
			Position++;
		}

		// If this is a parameter pin
		if (!bIsReturnPin)
		{
			FString TagParamName;

			// Copy the parameter name
			while (Position < FunctionTooltip.Len() && !FChar::IsWhitespace(FunctionTooltip[Position]))
			{
				TagParamName.AppendChar(FunctionTooltip[Position++]);
			}

			// If this @param tag doesn't match the param we're looking for
			if (TagParamName != PropertyName)
			{
				continue;
			}
		}

		// Advance past whitespace
		while (Position < FunctionTooltip.Len() && FChar::IsWhitespace(FunctionTooltip[Position]))
		{
			Position++;
		}

		FString PropertyTooltip;
		while (Position < FunctionTooltip.Len() && FunctionTooltip[Position] != TEXT('@'))
		{
			// advance past newline
			while (Position < FunctionTooltip.Len() && FChar::IsLinebreak(FunctionTooltip[Position]))
			{
				Position++;

				// advance past whitespace at the start of a new line
				while (Position < FunctionTooltip.Len() && FChar::IsWhitespace(FunctionTooltip[Position]))
				{
					Position++;
				}

				// replace the newline with a single space
				if (Position < FunctionTooltip.Len() && !FChar::IsLinebreak(FunctionTooltip[Position]))
				{
					PropertyTooltip.AppendChar(TEXT(' '));
				}
			}

			if (Position < FunctionTooltip.Len() && FunctionTooltip[Position] != TEXT('@'))
			{
				PropertyTooltip.AppendChar(FunctionTooltip[Position++]);
			}
		}

		// Trim any trailing whitespace from the descriptive text
		PropertyTooltip.TrimEndInline();

		// If we came up with a valid description for the param/return-val
		if (PropertyTooltip.IsEmpty())
		{
			continue;
		}

		return PropertyTooltip;
	}

	return FunctionTooltip;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
TMap<FName, FString> FVoxelUtilities::GetMetadata(const UObject* Object)
{
	if (!ensure(Object))
	{
		return {};
	}

	return Object->GetPackage()->GetMetaData()->ObjectMetaDataMap.FindRef(Object);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FString FVoxelUtilities::SanitizeCategory(const FString& Category)
{
	return FString::Join(ParseCategory(Category), TEXT("|"));
}

TArray<FString> FVoxelUtilities::ParseCategory(const FString& Category)
{
	TArray<FString> Categories;
	Category.ParseIntoArray(Categories, TEXT("|"));

	for (FString& SubCategory : Categories)
	{
		SubCategory = FName::NameToDisplayString(SubCategory.TrimStartAndEnd(), false);
		// Check is canon
		ensure(SubCategory == FName::NameToDisplayString(SubCategory.TrimStartAndEnd(), false));
	}

	if (Categories.Num() == 0)
	{
		ensure(Category.IsEmpty());
		Categories.Add("");
	}

	return Categories;
}

FString FVoxelUtilities::MakeCategory(const TArray<FString>& Categories)
{
	return SanitizeCategory(FString::Join(Categories, TEXT("|")));
}

bool FVoxelUtilities::IsSubCategory(const FString& Category, const FString& SubCategory)
{
	const TArray<FString> Categories = ParseCategory(Category);
	const TArray<FString> SubCategories = ParseCategory(SubCategory);
	if (Categories.Num() > SubCategories.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Categories.Num(); Index++)
	{
		if (Categories[Index] != SubCategories[Index])
		{
			return false;
		}
	}
	return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void FVoxelUtilities::CreateNewAsset_Deferred(UClass* Class, const FString& BaseName, const FString& Suffix, TFunction<void(UObject*)> SetupObject)
{
	// Create an appropriate and unique name
	FString AssetName;
	FString PackageName;

	const FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(BaseName, Suffix, PackageName, AssetName);

	IVoxelFactory* Factory = IVoxelAutoFactoryInterface::GetInterface().MakeFactory(Class);
	if (!ensure(Factory))
	{
		return;
	}

	Factory->OnSetupObject.AddLambda(SetupObject);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	ContentBrowser.FocusPrimaryContentBrowser(false);
	ContentBrowser.CreateNewAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), Class, Factory->GetUFactory());
}

UObject* FVoxelUtilities::CreateNewAsset_Direct(UClass* Class, const FString& BaseName, const FString& Suffix)
{
	FString NewPackageName;
	FString NewAssetName;

	const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(BaseName, Suffix, NewPackageName, NewAssetName);

	UPackage* Package = CreatePackage(*NewPackageName);
	if (!ensure(Package))
	{
		return nullptr;
	}

	UObject* Object = NewObject<UObject>(Package, Class, *NewAssetName, RF_Public | RF_Standalone);
	if (!ensure(Object))
	{
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Object);
	(void)Object->MarkPackageDirty();
	return Object;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void FVoxelUtilities::FocusObject(const UObject* Object)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensureVoxelSlow(Object))
	{
		return;
	}

	bool bFunctionSuccessful = false;
	for (const TFunction<bool(const UObject&)>& Function : GVoxelTryFocusObjectFunctions)
	{
		if (Function(*Object))
		{
			ensure(!bFunctionSuccessful);
			bFunctionSuccessful = true;
		}
	}
	if (bFunctionSuccessful)
	{
		return;
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(ConstCast(Actor), true, false, true);
		GEditor->NoteSelectionChange();
		GEditor->MoveViewportCamerasToActor(*ConstCast(Actor), false);
		return;
	}

	if (const UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectComponent(ConstCast(Component), true, false, true);
		GEditor->NoteSelectionChange();

		if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			GEditor->MoveViewportCamerasToComponent(SceneComponent, false);
		}

		return;
	}

	if (!Object->HasAnyFlags(RF_Transient))
	{
		// If we are a subobject of an asset, use the asset
		Object = Object->GetOutermostObject();
	}

	// Check if we can open an asset editor (OpenEditorForAsset return value is unreliable)
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Object->GetClass()).Pin();
	if (!AssetTypeActions)
	{
		return;
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
}

void FVoxelUtilities::FocusObject(const UObject& Object)
{
	FocusObject(&Object);
}
#endif

void FVoxelUtilities::InvokeFunctionWithNoParameters(UObject* Object, UFunction* Function)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Object) ||
		!ensure(Function) ||
		!ensure(!Function->Children))
	{
		return;
	}

	TGuardValue<bool> Guard(GAllowActorScriptExecutionInEditor, true);

	void* Params = nullptr;
	if (Function->ParmsSize > 0)
	{
		// Return value
		Params = FMemory::Malloc(Function->ParmsSize);
	}
	Object->ProcessEvent(Function, Params);
	FMemory::Free(Params);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelUtilities::ShouldSerializeBulkData(FArchive& Ar)
{
	return
		Ar.IsPersistent() &&
		!Ar.IsObjectReferenceCollector() &&
		!Ar.ShouldSkipBulkData() &&
		ensure(!Ar.IsTransacting());
}

void FVoxelUtilities::SerializeBulkData(
	UObject* Object,
	FByteBulkData& BulkData,
	FArchive& Ar,
	TFunctionRef<void()> SaveBulkData,
	TFunctionRef<void()> LoadBulkData)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ShouldSerializeBulkData(Ar))
	{
		return;
	}

	if (Ar.IsSaving())
	{
		// Clear the bulk data before writing it
		BulkData.RemoveBulkData();

		{
			VOXEL_SCOPE_COUNTER("SaveBulkData");
			SaveBulkData();
		}
	}

	BulkData.Serialize(Ar, Object);

	// NOTE: we can't call RemoveBulkData after saving as serialization is queued until the end of the save process

	if (Ar.IsLoading())
	{
		{
			VOXEL_SCOPE_COUNTER("LoadBulkData");
			LoadBulkData();
		}

		// Clear bulk data after loading to save memory
		BulkData.RemoveBulkData();
	}
}

void FVoxelUtilities::SerializeBulkData(
	UObject* Object,
	FByteBulkData& BulkData,
	FArchive& Ar,
	TFunctionRef<void(FArchive&)> Serialize)
{
	SerializeBulkData(
		Object,
		BulkData,
		Ar,
		[&]
		{
			FBulkDataWriter Writer(BulkData, true);
			Serialize(Writer);
		},
		[&]
		{
			FBulkDataReader Reader(BulkData, true);
			Serialize(Reader);
			ensure(!Reader.IsError() && Reader.AtEnd());
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32 FVoxelUtilities::HashProperty(const FProperty& Property, const void* DataPtr)
{
	switch (Property.GetCastFlags())
	{
#define CASE(Type) case Type::StaticClassCastFlags(): return FVoxelUtilities::MurmurHash(CastFieldChecked<Type>(Property).GetPropertyValue(DataPtr));
	CASE(FBoolProperty);
	CASE(FByteProperty);
	CASE(FIntProperty);
	CASE(FFloatProperty);
	CASE(FDoubleProperty);
	CASE(FUInt16Property);
	CASE(FUInt32Property);
	CASE(FUInt64Property);
	CASE(FInt8Property);
	CASE(FInt16Property);
	CASE(FInt64Property);
	CASE(FClassProperty);
	CASE(FNameProperty);
	CASE(FObjectProperty);
#if VOXEL_ENGINE_VERSION < 504
	CASE(FObjectPtrProperty);
#endif
	CASE(FWeakObjectProperty);
#undef CASE
	default: break;
	}

	if (Property.IsA<FArrayProperty>())
	{
		const FArrayProperty& ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		FScriptArrayHelper ArrayHelper(&ArrayProperty, DataPtr);

		uint32 Hash = FVoxelUtilities::MurmurHash(ArrayHelper.Num());
		for (int32 Index = 0; Index < ArrayHelper.Num(); Index++)
		{
			Hash ^= FVoxelUtilities::MurmurHash(HashProperty(*ArrayProperty.Inner, ArrayHelper.GetRawPtr(Index)), Index);
		}
		return Hash;
	}

	if (Property.IsA<FSetProperty>())
	{
		const FSetProperty& SetProperty = CastFieldChecked<FSetProperty>(Property);
		FScriptSetHelper SetHelper(&SetProperty, DataPtr);

		uint32 Hash = FVoxelUtilities::MurmurHash(SetHelper.Num());
		for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); Index++)
		{
			if (!SetHelper.IsValidIndex(Index))
			{
				continue;
			}

			Hash ^= FVoxelUtilities::MurmurHash(HashProperty(*SetProperty.ElementProp, SetHelper.GetElementPtr(Index)));
		}
		return Hash;
	}

	if (Property.IsA<FMapProperty>())
	{
		const FMapProperty& MapProperty = CastFieldChecked<FMapProperty>(Property);
		FScriptMapHelper MapHelper(&MapProperty, DataPtr);

		uint32 Hash = FVoxelUtilities::MurmurHash(MapHelper.Num());
		for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); Index++)
		{
			if (!MapHelper.IsValidIndex(Index))
			{
				continue;
			}

			Hash ^= FVoxelUtilities::MurmurHashMulti(
				HashProperty(*MapProperty.KeyProp, MapHelper.GetKeyPtr(Index)),
				HashProperty(*MapProperty.ValueProp, MapHelper.GetValuePtr(Index)));
		}
		return Hash;
	}

	if (!ensure(Property.IsA<FStructProperty>()))
	{
		return 0;
	}

	const UScriptStruct* Struct = CastFieldChecked<FStructProperty>(Property).Struct;

#define CASE(Type) \
	if (Struct == StaticStructFast<Type>()) \
	{ \
		return FVoxelUtilities::MurmurHash(*static_cast<const Type*>(DataPtr)); \
	}

	CASE(FVector2D);
	CASE(FVector);
	CASE(FVector4);
	CASE(FIntPoint);
	CASE(FIntVector);
	CASE(FRotator);
	CASE(FQuat);
	CASE(FTransform);
	CASE(FColor);
	CASE(FLinearColor);
	CASE(FMatrix);

#undef CASE

	UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
	check(CppStructOps);

	if (!ensure(CppStructOps->HasGetTypeHash()))
	{
		return 0;
	}

	return CppStructOps->GetStructTypeHash(DataPtr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if !UE_BUILD_SHIPPING
TMap<const UScriptStruct*, UScriptStruct::ICppStructOps*> GVoxelCachedStructOps;

VOXEL_RUN_ON_STARTUP_GAME()
{
	GVoxelCachedStructOps.Reserve(8192);

	ForEachObjectOfClass<UScriptStruct>([&](const UScriptStruct& Struct)
	{
		GVoxelCachedStructOps.Add(&Struct, Struct.GetCppStructOps());
	});
}
#endif

void FVoxelUtilities::DestroyStruct_Safe(const UScriptStruct* Struct, void* StructMemory)
{
	check(Struct);
	check(StructMemory);

	if (UObjectInitialized())
	{
		Struct->DestroyStruct(StructMemory);
	}
	else
	{
#if !UE_BUILD_SHIPPING
		// Always cleanly destroy structs so that leak detection works properly
		UScriptStruct::ICppStructOps* CppStructOps = GVoxelCachedStructOps[Struct];
		check(CppStructOps);
		if (CppStructOps->HasDestructor())
		{
			CppStructOps->Destruct(StructMemory);
		}
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::AddStructReferencedObjects(FReferenceCollector& Collector, const FVoxelStructView& StructView)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(StructView.IsValid()))
	{
		return;
	}

	if (StructView.GetStruct()->StructFlags & STRUCT_AddStructReferencedObjects)
	{
		StructView.GetStruct()->GetCppStructOps()->AddStructReferencedObjects()(StructView.GetMemory(), Collector);
	}

	for (TPropertyValueIterator<const FObjectProperty> It(StructView.GetStruct(), StructView.GetMemory()); It; ++It)
	{
		TObjectPtr<UObject>* ObjectPtr = static_cast<TObjectPtr<UObject>*>(ConstCast(It.Value()));
		Collector.AddReferencedObject(*ObjectPtr);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const FBoolProperty& FVoxelUtilities::MakeBoolProperty()
{
	static const TUniquePtr<FBoolProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FBoolProperty> Result = MakeUnique<FBoolProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->ElementSize = sizeof(bool);
		return Result;
	};
	return *Property;
}

const FFloatProperty& FVoxelUtilities::MakeFloatProperty()
{
	static const TUniquePtr<FFloatProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FFloatProperty> Result = MakeUnique<FFloatProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->ElementSize = sizeof(float);
		return Result;
	};
	return *Property;
}

const FIntProperty& FVoxelUtilities::MakeIntProperty()
{
	static const TUniquePtr<FIntProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FIntProperty> Result = MakeUnique<FIntProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->ElementSize = sizeof(int32);
		return Result;
	};
	return *Property;
}

const FNameProperty& FVoxelUtilities::MakeNameProperty()
{
	static const TUniquePtr<FNameProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FNameProperty> Result = MakeUnique<FNameProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->ElementSize = sizeof(FName);
		return Result;
	};
	return *Property;
}

TUniquePtr<FEnumProperty> FVoxelUtilities::MakeEnumProperty(const UEnum* Enum)
{
	TUniquePtr<FEnumProperty> Property = MakeUnique<FEnumProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->SetEnum(ConstCast(Enum));
	Property->ElementSize = sizeof(uint8);
	return Property;
}

TUniquePtr<FStructProperty> FVoxelUtilities::MakeStructProperty(const UScriptStruct* Struct)
{
	check(Struct);

	TUniquePtr<FStructProperty> Property = MakeUnique<FStructProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->Struct = ConstCast(Struct);
	Property->ElementSize = Struct->GetStructureSize();
	return Property;
}

TUniquePtr<FObjectProperty> FVoxelUtilities::MakeObjectProperty(const UClass* Class)
{
	check(Class);

	TUniquePtr<FObjectProperty> Property = MakeUnique<FObjectProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->PropertyClass = ConstCast(Class);
	Property->ElementSize = sizeof(UObject*);
	return Property;
}

TUniquePtr<FArrayProperty> FVoxelUtilities::MakeArrayProperty(FProperty* InnerProperty)
{
	check(InnerProperty);

	TUniquePtr<FArrayProperty> Property = MakeUnique<FArrayProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->Inner = InnerProperty;
	return Property;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelUtilities::PropertyFromText_Direct(const FProperty& Property, const FString& Text, void* Data, UObject* Owner)
{
	return Property.ImportText_Direct(*Text, Data, Owner, PPF_None) != nullptr;
}

bool FVoxelUtilities::PropertyFromText_InContainer(const FProperty& Property, const FString& Text, void* ContainerData, UObject* Owner)
{
	return Property.ImportText_InContainer(*Text, ContainerData, Owner, PPF_None) != nullptr;
}

bool FVoxelUtilities::PropertyFromText_InContainer(const FProperty& Property, const FString& Text, UObject* Owner)
{
	check(Owner);
	check(Owner->GetClass()->IsChildOf(CastChecked<UClass>(Property.Owner.ToUObject())));

	return PropertyFromText_InContainer(Property, Text, static_cast<void*>(Owner), Owner);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelUtilities::PropertyToText_Direct(const FProperty& Property, const void* Data, const UObject* Owner)
{
	VOXEL_FUNCTION_COUNTER();

	FString Value;
	ensure(Property.ExportText_Direct(Value, Data, Data, ConstCast(Owner), PPF_None));
	return Value;
}

FString FVoxelUtilities::PropertyToText_InContainer(const FProperty& Property, const void* ContainerData, const UObject* Owner)
{
	VOXEL_FUNCTION_COUNTER();

	FString Value;
	ensure(Property.ExportText_InContainer(0, Value, ContainerData, ContainerData, ConstCast(Owner), PPF_None));
	return Value;
}

FString FVoxelUtilities::PropertyToText_InContainer(const FProperty& Property, const UObject* Owner)
{
	check(Owner);
	check(Owner->GetClass()->IsChildOf(CastChecked<UClass>(Property.Owner.ToUObject())));

	return PropertyToText_InContainer(Property, static_cast<const void*>(Owner), Owner);
}