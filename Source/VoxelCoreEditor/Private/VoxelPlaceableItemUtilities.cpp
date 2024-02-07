// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPlaceableItemUtilities.h"
#include "VoxelEditorMinimal.h"
#include "ToolMenus.h"
#include "IPlacementModeModule.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"

constexpr const TCHAR* VoxelPlaceableItemHandle = TEXT("Voxel");

struct FVoxelPlaceableItemUtilitiesInfo
{
	bool bRegistered = false;

	void Register()
	{
		if (bRegistered)
		{
			return;
		}
		bRegistered = true;

		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

		const FPlacementCategoryInfo PlacementCategoryInfo(INVTEXT("Voxel"), FSlateIcon(FVoxelEditorStyle::GetStyleSetName(), "VoxelIcon"), VoxelPlaceableItemHandle, TEXT("PMVoxel"), 25);
		PlacementModeModule.RegisterPlacementCategory(PlacementCategoryInfo);
	}
};
FVoxelPlaceableItemUtilitiesInfo VoxelPlaceableItemInfo;

void FVoxelPlaceableItemUtilities::RegisterActorFactory(const UClass* ActorFactoryClass)
{
	VoxelPlaceableItemInfo.Register();

	UActorFactory* ActorFactory = ActorFactoryClass->GetDefaultObject<UActorFactory>();

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	PlacementModeModule.RegisterPlaceableItem(VoxelPlaceableItemHandle, MakeShared<FPlaceableItem>(ActorFactory, FAssetData(ActorFactory->NewActorClass)));
	PlacementModeModule.RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
}

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterVoxelPlaceableItems)
{
	VOXEL_FUNCTION_COUNTER();

	VoxelPlaceableItemInfo.Register();

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	ForEachObjectOfClass<UClass>([&](UClass& Class)
	{
		if (Class.HasMetaDataHierarchical(STATIC_FNAME("VoxelPlaceableItem")))
		{
			UActorFactory* Factory = nullptr;
			if (Class.IsChildOf<AVolume>())
			{
				// Make sure the box volume factory is used for volumes
				// TODO what about other actor types?
				Factory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), &Class);
			}
			PlacementModeModule.RegisterPlaceableItem(VoxelPlaceableItemHandle, MakeVoxelShared<FPlaceableItem>(Factory, FAssetData(&Class)));
		}
	});

	PlacementModeModule.RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
}

VOXEL_RUN_ON_STARTUP_EDITOR(UpdateVoxelPlaceableItemsSubMenus)
{
	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AddQuickMenu");
	if (!ensure(MainMenu))
	{
		return;
	}

	FToolMenuSection* DefaultSection = MainMenu->FindSection(TEXT("CreateAllCategories"));
	if (!ensure(DefaultSection))
	{
		return;
	}

	DefaultSection->Construct = MakeLambdaDelegate([OldConstruct = DefaultSection->Construct](UToolMenu* Menu)
	{
		OldConstruct.NewToolMenuDelegate.ExecuteIfBound(Menu);

		FToolMenuSection* Section = Menu->FindSection(TEXT("PMQCreateMenu"));
		if (!ensure(Section))
		{
			return;
		}

		FToolMenuEntry* Entry = Section->FindEntry(VoxelPlaceableItemHandle);
		if (!Entry)
		{
			return;
		}

		Entry->SubMenuData.ConstructMenu = MakeLambdaDelegate([OldMenuConstruct = Entry->SubMenuData.ConstructMenu](UToolMenu* VoxelMenu)
		{
			OldMenuConstruct.NewToolMenu.ExecuteIfBound(VoxelMenu);

			FToolMenuSection* VoxelSection = VoxelMenu->FindSection(VoxelPlaceableItemHandle);
			if (!ensure(VoxelSection))
			{
				return;
			}

			TArray<TSharedPtr<FPlaceableItem>> Items;
			IPlacementModeModule::Get().GetItemsForCategory(VoxelPlaceableItemHandle, Items);

			TMap<FName, TArray<FToolMenuEntry>> CategorizedEntries;
			for (const TSharedPtr<FPlaceableItem>& Item : Items)
			{
				const UClass* AssetClass = Cast<UClass>(Item->AssetData.GetAsset());
				if (!ensure(AssetClass) ||
					!AssetClass->HasMetaDataHierarchical(STATIC_FNAME("PlaceableSubMenu")))
				{
					continue;
				}

				const FToolMenuEntry* AssetEntry = VoxelSection->FindEntry(Item->AssetData.AssetName);
				if (!ensure(AssetEntry))
				{
					continue;
				}

				FString SubMenuName;
				ensure(AssetClass->GetStringMetaDataHierarchical(STATIC_FNAME("PlaceableSubMenu"), &SubMenuName));

				CategorizedEntries.FindOrAdd(*SubMenuName).Add(*AssetEntry);

				VoxelSection->Blocks.RemoveAll([SubMenuName, Item](const FToolMenuEntry& Block)
				{
					return Block.Name == Item->AssetData.AssetName;
				});
			}

			for (const auto& It : CategorizedEntries)
			{
				const FName Name = It.Key;
				const TArray<FToolMenuEntry>& Entries = It.Value;

				VoxelSection->AddSubMenu(Name,
					FText::FromName(Name),
					{},
					MakeLambdaDelegate([Name, Entries](UToolMenu* VoxelSubMenu)
					{
						FToolMenuSection* SubMenuSection = VoxelSubMenu->FindSection(Name);
						if (SubMenuSection == nullptr)
						{
							SubMenuSection = &VoxelSubMenu->AddSection(Name, FText::FromName(Name));
						}

						for (const FToolMenuEntry& MenuEntry : Entries)
						{
							SubMenuSection->AddEntry(MenuEntry);
						}
					}));
			}
		});
	});
}