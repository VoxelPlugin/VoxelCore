// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelAssetTypeActions.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Toolkits/VoxelEditorToolkitImpl.h"

EAssetTypeCategories::Type GVoxelAssetCategory;

VOXEL_RUN_ON_STARTUP(RegisterVoxelAssetTypes, Editor, 999)
{
	VOXEL_FUNCTION_COUNTER();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	GVoxelAssetCategory = AssetTools.RegisterAdvancedAssetCategory("Voxel", INVTEXT("Voxel"));

	// Make a copy as RegisterAssetTypeActions creates UObjects
	TVoxelArray<UClass*> Classes;
	ForEachObjectOfClass<UClass>([&](UClass& Class)
	{
		if (!Class.HasAnyClassFlags(CLASS_Abstract) &&
			Class.HasMetaDataHierarchical(STATIC_FNAME("VoxelAssetType")))
		{
			Classes.Add(&Class);
		}
	});

	for (UClass* Class : Classes)
	{
		FVoxelAssetTypeActions::Register(Class, MakeVoxelShared<FVoxelAssetTypeActions>());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32 FVoxelAssetTypeActionsBase::GetCategories()
{
	return GVoxelAssetCategory;
}

void FVoxelAssetTypeActionsBase::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	UClass* Class = GetSupportedClass();

	bool bCanReimport = true;
	for (UObject* Object : InObjects)
	{
		if (!Object || !Object->IsA(Class))
		{
			return;
		}

		bCanReimport &= FReimportManager::Instance()->CanReimport(Object);
	}

	if (bCanReimport)
	{
		MenuBuilder.AddMenuEntry(
			INVTEXT("Reimport"),
			INVTEXT("Reimport the selected asset(s)."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
			FUIAction(MakeLambdaDelegate([=]
			{
				for (UObject* Object : InObjects)
				{
					FReimportManager::Instance()->Reimport(Object, true);
				}
			})));
	}

	for (UFunction* Function : GetClassFunctions(Class))
	{
		if (!Function->HasMetaData(STATIC_FNAME("ShowInContextMenu")))
		{
			continue;
		}
		if (Function->Children)
		{
			ensureMsgf(false, TEXT("Function %s has ShowInContextMenu but has parameters!"), *Function->GetDisplayNameText().ToString());
			Function->RemoveMetaData(STATIC_FNAME("ShowInContextMenu"));
			return;
		}

		MenuBuilder.AddMenuEntry(
			Function->GetDisplayNameText(),
			Function->GetToolTipText(),
			FSlateIcon(NAME_None, NAME_None),
			FUIAction(MakeLambdaDelegate([=]
			{
				for (UObject* Object : InObjects)
				{
					FVoxelObjectUtilities::InvokeFunctionWithNoParameters(Object, Function);
				}
			})));
	}
}

void FVoxelAssetTypeActionsBase::OpenAssetEditor(const TArray<UObject*>& InObjects, const TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Object : InObjects)
	{
		if (!ensure(Object))
		{
			continue;
		}

		const TSharedPtr<FVoxelEditorToolkitImpl> NewVoxelEditor = MakeToolkit();
		if (!NewVoxelEditor)
		{
			FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
			return;
		}

		NewVoxelEditor->InitVoxelEditor(EditWithinLevelEditor, Object);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelEditorToolkitImpl> FVoxelAssetTypeActions::MakeToolkit() const
{
	return FVoxelEditorToolkitImpl::MakeToolkit(Class);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAssetTypeActions::Register(UClass* Class, const TSharedRef<FVoxelAssetTypeActions>& Action)
{
	VOXEL_FUNCTION_COUNTER();
	check(Class);

	Action->Class = Class;

	FColor Color = FColor::Black;
	{
		FString AssetColor;
		if (Class->GetStringMetaDataHierarchical("AssetColor", &AssetColor))
		{
			if (AssetColor == "Orange")
			{
				Color = FColor(255, 140, 0);
			}
			else if (AssetColor == "DarkGreen")
			{
				Color = FColor(0, 192, 0);
			}
			else if (AssetColor == "LightGreen")
			{
				Color = FColor(128, 255, 128);
			}
			else if (AssetColor == "Blue")
			{
				Color = FColor(0, 175, 255);
			}
			else if (AssetColor == "LightBlue")
			{
				Color = FColor(0, 175, 175);
			}
			else if (AssetColor == "Red")
			{
				Color = FColor(128, 0, 64);
			}
			else
			{
				ensure(false);
			}
		}
	}
	Action->Color = Color;

	FString AssetSubMenu;
	if (Class->GetStringMetaDataHierarchical("AssetSubMenu", &AssetSubMenu))
	{
		TArray<FString> SubMenuStrings;
		AssetSubMenu.ParseIntoArray(SubMenuStrings, TEXT("."));

		for (const FString& SubMenu : SubMenuStrings)
		{
			if (!ensure(!SubMenu.IsEmpty()))
			{
				continue;
			}

			Action->SubMenus.Add(FText::FromString(SubMenu));
		}
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(Action);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstanceAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FVoxelAssetTypeActions::GetActions(InObjects, MenuBuilder);

	MenuBuilder.AddMenuEntry(
		INVTEXT("Create instance"),
		INVTEXT("Creates a new " + GetInstanceClass()->GetDisplayNameText().ToString() + " using this asset as parent"),
		GetInstanceActionIcon(),
		FUIAction(MakeWeakPtrDelegate(this, [this, InObjects]
		{
			CreateNewInstances(InObjects);
		})));
}

void FVoxelInstanceAssetTypeActions::CreateNewInstances(const TArray<UObject*>& ParentAssets) const
{
	IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<UObject*> ObjectsToSync;
	for (UObject* ParentAsset : ParentAssets)
	{
		if (!ensure(ParentAsset))
		{
			continue;
		}

		FString NewAssetName;
		FString PackageName;
		CreateUniqueAssetName(ParentAsset->GetPackage()->GetName(), "_Inst", PackageName, NewAssetName);

		UObject* InstanceAsset = AssetToolsModule.CreateAsset(NewAssetName, FPackageName::GetLongPackagePath(PackageName), GetInstanceClass(), nullptr);
		if (!ensure(InstanceAsset))
		{
			continue;
		}

		SetParent(InstanceAsset, ParentAsset);
		InstanceAsset->PostEditChange();

		ObjectsToSync.Add(InstanceAsset);
	}

	if (ObjectsToSync.Num() == 0)
	{
		return;
	}

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	ContentBrowserModule.SyncBrowserToAssets(ObjectsToSync);
}