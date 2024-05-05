// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelPropertyType.h"

struct FVoxelPropertyTypeSelectorRow;
using SVoxelPropertyTypeTreeView = STreeView<TSharedPtr<FVoxelPropertyTypeSelectorRow>>;

struct FVoxelPropertyTypeSelectorRow
{
public:
	FString Name;
	FVoxelPropertyType Type;
	TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>> Children;

	FVoxelPropertyTypeSelectorRow() = default;

	explicit FVoxelPropertyTypeSelectorRow(const FVoxelPropertyType& PinType)
		: Name(PinType.GetInnerType().ToString())
		, Type(PinType)
	{
	}

	explicit FVoxelPropertyTypeSelectorRow(const FString& Name)
		: Name(Name)
	{
	}

	FVoxelPropertyTypeSelectorRow(
		const FString& Name,
		const TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>>& Children)
		: Name(Name)
		, Children(Children)
	{
	}
};

class SVoxelPropertyTypeSelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTypeChanged, FVoxelPropertyType)

public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(TVoxelSet<FVoxelPropertyType>, AllowedTypes)
		SLATE_EVENT(FOnTypeChanged, OnTypeChanged)
		SLATE_EVENT(FSimpleDelegate, OnCloseMenu)
	};

	void Construct(const FArguments& InArgs);

public:
	void ClearSelection() const;
	TSharedPtr<SWidget> GetWidgetToFocus() const;

private:
	TSharedRef<ITableRow> GenerateTypeTreeRow(TSharedPtr<FVoxelPropertyTypeSelectorRow> RowItem, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnTypeSelectionChanged(TSharedPtr<FVoxelPropertyTypeSelectorRow> Selection, ESelectInfo::Type SelectInfo) const;
	void GetTypeChildren(TSharedPtr<FVoxelPropertyTypeSelectorRow> PinTypeRow, TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>>& PinTypeRows) const;

	void OnFilterTextChanged(const FText& NewText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo) const;

private:
	void GetChildrenMatchingSearch(const FText& InSearchText);
	void FillTypesList();

private:
	TSharedPtr<SVoxelPropertyTypeTreeView> TypeTreeView;
	TSharedPtr<SSearchBox> FilterTextBox;

private:
	TAttribute<TVoxelSet<FVoxelPropertyType>> AllowedTypes;
	FOnTypeChanged OnTypeChanged;
	FSimpleDelegate OnCloseMenu;

private:
	FText SearchText;

	TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>> TypesList;
	TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>> FilteredTypesList;

	int32 FilteredTypesCount = 0;
	int32 TotalTypesCount = 0;
};