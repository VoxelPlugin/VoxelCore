// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "EdGraph/EdGraph.h"
#include "UObject/MetaData.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectThreadContext.h"
#include "Misc/UObjectToken.h"
#include "StructUtils/InstancedStruct.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "ObjectTools.h"
#include "PackageTools.h"
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
TVoxelArray<TVoxelUniqueFunction<bool(const UObject&)>> GVoxelTryFocusObjectFunctions;
#endif
TVoxelArray<TVoxelUniqueFunction<FString(const UObject&)>> GVoxelTryGetObjectNameFunctions;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

using FVoxelSerializeVoxelVersion = void(*)(FArchive&);
VOXELCORE_API FVoxelSerializeVoxelVersion SerializeVoxelVersionPtr;

void SerializeVoxelVersion(FArchive& Ar)
{
	if (SerializeVoxelVersionPtr)
	{
		SerializeVoxelVersionPtr(Ar);
	}
}

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

	return ParseStructTooltip(FunctionTooltip);
}

FString FVoxelUtilities::GetStructTooltip(const UStruct& Struct)
{
	return ParseStructTooltip(Struct.GetToolTipText().ToString());
}

FString FVoxelUtilities::ParseStructTooltip(FString Tooltip)
{
	if (Tooltip.IsEmpty())
	{
		return {};
	}

	static const FString DoxygenParam(TEXT("@param"));
	static const FString DoxygenReturn(TEXT("@return"));
	static const FString DoxygenSee(TEXT("@see"));
	static const FString TooltipSee(TEXT("See:"));
	static const FString DoxygenNote(TEXT("@note"));
	static const FString TooltipNote(TEXT("Note:"));

	Tooltip.Split(DoxygenParam, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	Tooltip.Split(DoxygenReturn, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);

	Tooltip.ReplaceInline(*DoxygenSee, *TooltipSee);
	Tooltip.ReplaceInline(*DoxygenNote, *TooltipNote);

	Tooltip.TrimStartAndEndInline();
	return Tooltip;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
FString FVoxelUtilities::GetFunctionType(const FProperty& Property)
{
	// TInstancedStruct
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == FInstancedStruct::StaticStruct())
		{
			if (const FString* BaseStructPath = Property.FindMetaData("BaseStruct"))
			{
				const UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, **BaseStructPath);
				if (ensure(Struct))
				{
					return "TInstancedStruct<" + Struct->GetStructCPPName() + ">";
				}
			}

			return "FInstancedStruct";
		}
	}

	// Convert TObjectPtr to raw pointers for UFunctions

	FString ExtendedType;
	FString Type = Property.GetCPPType(&ExtendedType);

	if (ExtendedType.RemoveFromStart("TObjectPtr<"))
	{
		ensure(ExtendedType.RemoveFromEnd(">"));
		ExtendedType += "*";
	}
	if (Type.RemoveFromStart("TObjectPtr<"))
	{
		ensure(Type.RemoveFromEnd(">"));
		Type += "*";
	}
	if (ExtendedType.RemoveFromStart("<TObjectPtr<"))
	{
		ensure(ExtendedType.RemoveFromEnd("> >"));
		ExtendedType = "<" + ExtendedType + "*>";
	}

	Type += ExtendedType;

	return Type;
}

TMap<FName, FString> FVoxelUtilities::GetMetadata(const UObject* Object)
{
	if (!ensure(Object))
	{
		return {};
	}

#if VOXEL_ENGINE_VERSION < 506
	return Object->GetPackage()->GetMetaData()->ObjectMetaDataMap.FindRef(Object);
#else
	return Object->GetPackage()->GetMetaData().ObjectMetaDataMap.FindRef(Object);
#endif
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
void FVoxelUtilities::CreateNewAsset_Deferred(
	UClass* Class,
	const FString& BaseName,
	const TArray<FString>& PrefixesToRemove,
	const FString& NewPrefix,
	const FString& Suffix,
	TFunction<void(UObject*)> SetupObject)
{
	// Create an appropriate and unique name
	FString AssetName;
	FString PackageName;

	CreateUniqueAssetName(
		BaseName,
		PrefixesToRemove,
		NewPrefix,
		Suffix,
		PackageName,
		AssetName);

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

UObject* FVoxelUtilities::CreateNewAsset_Direct(
	UClass* Class,
	const FString& BaseName,
	const TArray<FString>& PrefixesToRemove,
	const FString& NewPrefix,
	const FString& Suffix)
{
	FString NewPackageName;
	FString NewAssetName;

	CreateUniqueAssetName(
		BaseName,
		PrefixesToRemove,
		NewPrefix,
		Suffix,
		NewPackageName,
		NewAssetName);

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

void FVoxelUtilities::CreateUniqueAssetName(
	const FString& BasePackageName,
	const TArray<FString>& PrefixesToRemove,
	const FString& NewPrefix,
	const FString& Suffix,
	FString& OutPackageName,
	FString& OutAssetName)
{
	const FString SanitizedBasePackageName = UPackageTools::SanitizePackageName(BasePackageName);

	const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName);
	FString BaseAssetName = FPackageName::GetLongPackageAssetName(SanitizedBasePackageName);
	for (const FString& Prefix : PrefixesToRemove)
	{
		BaseAssetName.RemoveFromStart(Prefix);
	}
	BaseAssetName = NewPrefix + BaseAssetName;

	const FString SanitizedBaseAssetName = ObjectTools::SanitizeObjectName(BaseAssetName);

	const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackagePath / SanitizedBaseAssetName, Suffix, OutPackageName, OutAssetName);
}

void FVoxelUtilities::CreateUniqueAssetName(
	const UObject* ObjectWithPath,
	const TArray<FString>& PrefixesToRemove,
	const FString& NewPrefix,
	const FString& Suffix,
	FString& OutPackageName,
	FString& OutAssetName)
{
	if (!ensure(ObjectWithPath))
	{
		return;
	}

	CreateUniqueAssetName(
		ObjectWithPath->GetPackage()->GetName(),
		PrefixesToRemove,
		NewPrefix,
		Suffix,
		OutPackageName,
		OutAssetName);
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
	for (const TVoxelUniqueFunction<bool(const UObject&)>& Function : GVoxelTryFocusObjectFunctions)
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

	if (const UEdGraph* EdGraph = Cast<UEdGraph>(Object))
	{
		if (const TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(EdGraph, true))
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EdGraph);
		}
	}

	if (const UEdGraphNode* EdGraphNode = Cast<UEdGraphNode>(Object))
	{
		if (const TSharedPtr<IBlueprintEditor> BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(EdGraphNode, true))
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EdGraphNode);
		}
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(ConstCast(Actor), true, false, true);
		GEditor->NoteSelectionChange();
		GEditor->MoveViewportCamerasToActor(*ConstCast(Actor), false);

		if (const TSharedPtr<FTabManager> LevelEditorTabManager = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTabManager())
		{
			FGlobalTabmanager::Get()->DrawAttentionToTabManager(LevelEditorTabManager.ToSharedRef());
		}
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

		if (const TSharedPtr<FTabManager> LevelEditorTabManager = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTabManager())
		{
			FGlobalTabmanager::Get()->DrawAttentionToTabManager(LevelEditorTabManager.ToSharedRef());
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

FString FVoxelUtilities::GetReadableName(const UObject* Object)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(IsInGameThread());

	if (!Object)
	{
		return "<null>";
	}

	{
		FString Result;
		for (const TVoxelUniqueFunction<FString(const UObject&)>& Function : GVoxelTryGetObjectNameFunctions)
		{
			const FString Name = Function(*Object);

			if (!Name.IsEmpty())
			{
				ensure(Result.IsEmpty());
				Result = Name;
			}
		}

		if (!Result.IsEmpty())
		{
			return Result;
		}
	}

	if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
	{
#if WITH_EDITOR
		FString Result = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		if (Result.TrimStartAndEnd().IsEmpty())
		{
			Result = "<empty>";
		}
		return Result;
#else
		return Node->GetName();
#endif
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		return Actor->GetActorNameOrLabel();
	}

	if (const UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		FString ActorName;
		if (const AActor* Owner = Component->GetOwner())
		{
			ActorName = Owner->GetActorNameOrLabel();
		}
		else
		{
			ActorName = "<null>";
		}

		return ActorName + "." + Component->GetName();
	}

	if (!Object->HasAnyFlags(RF_Transient))
	{
		// If we are a subobject of an asset, use the asset
		Object = Object->GetOutermostObject();
	}

	return Object->GetName();
}

FString FVoxelUtilities::GetReadableName(const UObject& Object)
{
	return GetReadableName(&Object);
}

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

FString FVoxelUtilities::GetArchivePath(const FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<FProperty*> Properties;
	Ar.GetSerializedPropertyChain(Properties);

	FString Name;
	if (const FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext())
	{
		if (SerializeContext->SerializedObject)
		{
			Name += SerializeContext->SerializedObject->GetPathName();
		}
	}

	for (int32 Index = 0; Index < Properties.Num(); Index++)
	{
		if (!Name.IsEmpty())
		{
			Name += ".";
		}
		Name += Properties.Last(Index)->GetNameCPP();
	}

	return Name;
}

bool FVoxelUtilities::ShouldSerializeBulkData(FArchive& Ar)
{
	return
		Ar.IsPersistent() &&
		!Ar.IsObjectReferenceCollector() &&
		!Ar.ShouldSkipBulkData() &&
		ensure(!Ar.IsTransacting());
}

void FVoxelUtilities::SerializeStruct(
	FArchive& Ar,
	UScriptStruct*& Struct)
{
	VOXEL_FUNCTION_COUNTER();

	FString StructPath;
	if (Ar.IsSaving() &&
		Struct)
	{
		StructPath = Struct->GetPathName();
	}

	Ar << Struct;
	Ar << StructPath;

	if (!Struct &&
		!StructPath.IsEmpty())
	{
		VOXEL_SCOPE_COUNTER("FindFirstObject");

		// Serializing structs directly doesn't seem to handle redirects properly
		const FCoreRedirectObjectName RedirectedName = FCoreRedirects::GetRedirectedName(
			ECoreRedirectFlags::Type_Struct,
			FCoreRedirectObjectName(StructPath),
			ECoreRedirectMatchFlags::AllowPartialMatch);

		Struct = FindFirstObject<UScriptStruct>(*RedirectedName.ToString(), EFindFirstObjectOptions::EnsureIfAmbiguous);

		if (!ensureVoxelSlow(Struct))
		{
			return;
		}
	}

	Ar.Preload(Struct);
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

			if (!Reader.IsError() &&
				Reader.AtEnd())
			{
				return;
			}

			// Don't raise error when cooking
			if (IsRunningCookCommandlet())
			{
				LOG_VOXEL(Warning, "Failed to serialize BulkData on %s", *Object->GetPathName());
			}
			else
			{
				ensureMsgf(false, TEXT("Failed to serialize BulkData on %s"), *Object->GetPathName());
			}
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* FVoxelUtilities::NewObject_Safe(UObject* Outer, const UClass* Class, const FName Name)
{
	const FName SafeName = MakeUniqueObjectName(Outer, Class, Name);
	return NewObject<UObject>(Outer, Class, SafeName);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint64 FVoxelUtilities::HashProperty(const FProperty& Property, const void* DataPtr)
{
	checkUObjectAccess();

	switch (Property.GetCastFlags())
	{
#define CASE(Type) case Type::StaticClassCastFlags(): return MurmurHash(CastFieldChecked<Type>(Property).GetPropertyValue(DataPtr));
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
#undef CASE

	case FStrProperty::StaticClassCastFlags():
	{
		return HashString(CastFieldChecked<FStrProperty>(Property).GetPropertyValue(DataPtr));
	}
	case FNameProperty::StaticClassCastFlags():
	{
		return HashString(CastFieldChecked<FNameProperty>(Property).GetPropertyValue(DataPtr).ToString());
	}
	case FEnumProperty::StaticClassCastFlags():
	{
		return HashProperty(*CastFieldChecked<FEnumProperty>(Property).GetUnderlyingProperty(), DataPtr);
	}
	case FClassProperty::StaticClassCastFlags():
	case FObjectProperty::StaticClassCastFlags():
	{
		const UObject* Object = CastFieldChecked<FObjectProperty>(Property).GetPropertyValue(DataPtr);
		if (!Object)
		{
			return 0;
		}

		ensure(
			!Object->HasAnyFlags(RF_Transient) ||
			Object->IsA<UClass>() ||
			Object->IsA<UEnum>() ||
			Object->IsA<UScriptStruct>());

		return HashString(Object->GetPathName());
	}
	case FWeakObjectProperty::StaticClassCastFlags():
	{
		const UObject* Object = CastFieldChecked<FWeakObjectProperty>(Property).GetPropertyValue(DataPtr).Get();
		if (!Object)
		{
			return 0;
		}

		ensure(
			!Object->HasAnyFlags(RF_Transient) ||
			Object->IsA<UClass>() ||
			Object->IsA<UEnum>() ||
			Object->IsA<UScriptStruct>());

		return HashString(Object->GetPathName());
	}
	case FArrayProperty::StaticClassCastFlags():
	{
		const FArrayProperty& ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		FScriptArrayHelper Helper(&ArrayProperty, DataPtr);

		TVoxelInlineArray<uint64, 64> Hashes;
		Hashes.Reserve(Helper.Num());

		for (int32 Index = 0; Index < Helper.Num(); Index++)
		{
			Hashes.Add_EnsureNoGrow(HashProperty(*ArrayProperty.Inner, Helper.GetRawPtr(Index)));
		}

		return MurmurHashView(Hashes, Helper.Num());
	}
	case FSetProperty::StaticClassCastFlags():
	{
		const FSetProperty& SetProperty = CastFieldChecked<FSetProperty>(Property);
		FScriptSetHelper Helper(&SetProperty, DataPtr);

		TVoxelInlineArray<uint64, 64> Hashes;
		Hashes.Reserve(Helper.Num());

		for (int32 Index = 0; Index < Helper.GetMaxIndex(); Index++)
		{
			if (!Helper.IsValidIndex(Index))
			{
				continue;
			}

			Hashes.Add_EnsureNoGrow(HashProperty(*SetProperty.ElementProp, Helper.GetElementPtr(Index)));
		}

		return MurmurHashView(Hashes, Helper.Num());
	}
	case FMapProperty::StaticClassCastFlags():
	{
		const FMapProperty& MapProperty = CastFieldChecked<FMapProperty>(Property);
		FScriptMapHelper Helper(&MapProperty, DataPtr);

		TVoxelInlineArray<uint64, 64> Hashes;
		Hashes.Reserve(2 * Helper.Num());

		for (int32 Index = 0; Index < Helper.GetMaxIndex(); Index++)
		{
			if (!Helper.IsValidIndex(Index))
			{
				continue;
			}

			Hashes.Add_EnsureNoGrow(HashProperty(*MapProperty.KeyProp, Helper.GetKeyPtr(Index)));
			Hashes.Add_EnsureNoGrow(HashProperty(*MapProperty.ValueProp, Helper.GetValuePtr(Index)));
		}

		return MurmurHashView(Hashes, Helper.Num());
	}
	case FStructProperty::StaticClassCastFlags():
	{
		const UScriptStruct* Struct = CastFieldChecked<FStructProperty>(Property).Struct;

		if (Struct == StaticStructFast<FVoxelInstancedStruct>())
		{
			const FVoxelInstancedStruct& InstancedStruct = *static_cast<const FVoxelInstancedStruct*>(DataPtr);
			if (!InstancedStruct.IsValid())
			{
				return 0;
			}

			Struct = InstancedStruct.GetScriptStruct();
			DataPtr = InstancedStruct.GetStructMemory();
		}

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

		TVoxelInlineArray<uint64, 64> Hashes;
		for (FProperty& ChildProperty : GetStructProperties(Struct))
		{
			Hashes.Add(HashProperty(ChildProperty, ChildProperty.ContainerPtrToValuePtr<void>(DataPtr)));
		}

		if (Hashes.Num() > 0)
		{
			return MurmurHashView(Hashes);
		}

		if (Struct->GetPropertiesSize() == 1)
		{
			// Empty struct
			return 0;
		}

		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps);

		if (!CppStructOps->HasGetTypeHash())
		{
			// TODO Serialize instead and compute hash from that?
			return 0;
		}

		return CppStructOps->GetStructTypeHash(DataPtr);
	}
	default:
	{
		ensure(false);
		return 0;
	}
	}
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

#if WITH_EDITOR
TVoxelUniqueFunction<void(const UScriptStruct* Struct, void* StructMemory)> GVoxelDestroyStructOverride;
#endif

void FVoxelUtilities::DestroyStruct_Safe(const UScriptStruct* Struct, void* StructMemory)
{
	check(Struct);
	check(StructMemory);

#if WITH_EDITOR
	if (GVoxelDestroyStructOverride &&
		IsInGameThread())
	{
		GVoxelDestroyStructOverride(Struct, StructMemory);
		return;
	}
#endif

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

	Collector.AddPropertyReferencesWithStructARO(StructView.GetScriptStruct(), StructView.GetStructMemory());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const FBoolProperty& FVoxelUtilities::MakeBoolProperty()
{
	static const TUniquePtr<FBoolProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FBoolProperty> Result = MakeUnique<FBoolProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->SetElementSize(sizeof(bool));
		return Result;
	};
	return *Property;
}

const FFloatProperty& FVoxelUtilities::MakeFloatProperty()
{
	static const TUniquePtr<FFloatProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FFloatProperty> Result = MakeUnique<FFloatProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->SetElementSize(sizeof(float));
		return Result;
	};
	return *Property;
}

const FIntProperty& FVoxelUtilities::MakeIntProperty()
{
	static const TUniquePtr<FIntProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FIntProperty> Result = MakeUnique<FIntProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->SetElementSize(sizeof(int32));
		return Result;
	};
	return *Property;
}

const FNameProperty& FVoxelUtilities::MakeNameProperty()
{
	static const TUniquePtr<FNameProperty> Property = INLINE_LAMBDA
	{
		TUniquePtr<FNameProperty> Result = MakeUnique<FNameProperty>(FFieldVariant(), FName(), EObjectFlags());
		Result->SetElementSize(sizeof(FName));
		return Result;
	};
	return *Property;
}

TUniquePtr<FEnumProperty> FVoxelUtilities::MakeEnumProperty(const UEnum* Enum)
{
	TUniquePtr<FEnumProperty> Property = MakeUnique<FEnumProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->SetEnum(ConstCast(Enum));
	Property->SetElementSize(sizeof(uint8));
	return Property;
}

TUniquePtr<FStructProperty> FVoxelUtilities::MakeStructProperty(const UScriptStruct* Struct)
{
	check(Struct);

	TUniquePtr<FStructProperty> Property = MakeUnique<FStructProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->Struct = ConstCast(Struct);
	Property->SetElementSize(Struct->GetStructureSize());
	return Property;
}

TUniquePtr<FObjectProperty> FVoxelUtilities::MakeObjectProperty(const UClass* Class)
{
	check(Class);

	TUniquePtr<FObjectProperty> Property = MakeUnique<FObjectProperty>(FFieldVariant(), FName(), EObjectFlags());
	Property->PropertyClass = ConstCast(Class);
	Property->SetElementSize(sizeof(UObject*));
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