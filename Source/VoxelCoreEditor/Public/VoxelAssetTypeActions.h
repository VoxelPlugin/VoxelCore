// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "AssetTypeActions_Base.h"

class FVoxelEditorToolkitImpl;

class VOXELCOREEDITOR_API FVoxelAssetTypeActionsBase : public FAssetTypeActions_Base
{
public:
	FVoxelAssetTypeActionsBase() = default;

	//~ Begin FAssetTypeActions_Base Interface
	virtual uint32 GetCategories() override;

	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;
	//~ End FAssetTypeActions_Base Interface

	virtual bool SupportReimport() const { return false; }
	virtual TSharedPtr<FVoxelEditorToolkitImpl> MakeToolkit() const { return nullptr; }
};

class VOXELCOREEDITOR_API FVoxelAssetTypeActions : public FVoxelAssetTypeActionsBase
{
public:
	FVoxelAssetTypeActions() = default;

	//~ Begin FVoxelAssetTypeActionsBase Interface
	virtual FText GetName() const override { return Class->GetDisplayNameText(); }
	virtual FColor GetTypeColor() const override { return Color; }
	virtual UClass* GetSupportedClass() const override { return Class; }
	virtual const TArray<FText>& GetSubMenus() const override { return SubMenus; }
	virtual TSharedPtr<FVoxelEditorToolkitImpl> MakeToolkit() const override;
	//~ End FVoxelAssetTypeActionsBase Interface

public:
	static void Register(UClass* Class, const TSharedRef<FVoxelAssetTypeActions>& Action);

private:
	UClass* Class = nullptr;
	FColor Color;
	TArray<FText> SubMenus;
};

class VOXELCOREEDITOR_API FVoxelInstanceAssetTypeActions : public FVoxelAssetTypeActions
{
public:
	using FVoxelAssetTypeActions::FVoxelAssetTypeActions;

	//~ Begin FVoxelAssetTypeActions Interface
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	//~ End FVoxelAssetTypeActions Interface

	virtual UClass* GetInstanceClass() const = 0;
	virtual FSlateIcon GetInstanceActionIcon() const = 0;
	virtual void SetParent(UObject* InstanceAsset, UObject* ParentAsset) const = 0;

private:
	void CreateNewInstances(const TArray<UObject*>& ParentAssets) const;
};