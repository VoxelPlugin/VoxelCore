// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelPropertyType.h"

class SVoxelPropertyTypeSelector;

class SVoxelPropertyTypeComboBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTypeChanged, FVoxelPropertyType)

public:
	VOXEL_SLATE_ARGS()
	{
		FArguments()
			: _AllowArray(true)
			, _DetailsWindow(true)
		{
		}

		SLATE_ATTRIBUTE(TVoxelSet<FVoxelPropertyType>, AllowedTypes)
		SLATE_ATTRIBUTE(FVoxelPropertyType, CurrentType)
		SLATE_ARGUMENT(bool, AllowArray)
		SLATE_ARGUMENT(bool, DetailsWindow)
		SLATE_ATTRIBUTE(bool, ReadOnly)
		SLATE_EVENT(FOnTypeChanged, OnTypeChanged)
	};

	void Construct(const FArguments& InArgs);

	//~ Begin SCompoundWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
	//~ End SCompoundWidget Interface

private:
	TSharedRef<SWidget> GetMenuContent();

	TSharedRef<SWidget> GetContainerTypeMenuContent();
	void OnContainerTypeSelectionChanged(EVoxelPropertyContainerType ContainerType);

	void UpdateType(const FVoxelPropertyType& NewType);

private:
	TSharedPtr<SComboButton> TypeComboButton;
	TSharedPtr<SComboButton> ContainerTypeComboButton;
	TSharedPtr<SImage> MainIcon;
	TSharedPtr<STextBlock> MainTextBlock;

	TSharedPtr<SMenuOwner> MenuContent;
	TSharedPtr<SMenuOwner> ContainerTypeMenuContent;
	TSharedPtr<SVoxelPropertyTypeSelector> TypeSelector;

private:
	bool bAllowArray = false;
	TAttribute<TVoxelSet<FVoxelPropertyType>> AllowedTypes;
	TAttribute<FVoxelPropertyType> CurrentType;

	FVoxelPropertyType CachedType;

	TAttribute<bool> ReadOnly;
	FOnTypeChanged OnTypeChanged;
};