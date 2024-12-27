// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelPropertyTypeSelector.h"
#include "SListViewSelectorDropdownMenu.h"

void SVoxelPropertyTypeSelector::Construct(const FArguments& InArgs)
{
	AllowedTypes = InArgs._AllowedTypes;

	OnTypeChanged = InArgs._OnTypeChanged;
	ensure(OnTypeChanged.IsBound());
	OnCloseMenu = InArgs._OnCloseMenu;

	FillTypesList();

	SAssignNew(TypeTreeView, SVoxelPropertyTypeTreeView)
	.TreeItemsSource(&FilteredTypesList)
	.SelectionMode(ESelectionMode::Single)
	.OnGenerateRow(this, &SVoxelPropertyTypeSelector::GenerateTypeTreeRow)
	.OnSelectionChanged(this, &SVoxelPropertyTypeSelector::OnTypeSelectionChanged)
	.OnGetChildren(this, &SVoxelPropertyTypeSelector::GetTypeChildren);

	SAssignNew(FilterTextBox, SSearchBox)
	.OnTextChanged(this, &SVoxelPropertyTypeSelector::OnFilterTextChanged)
	.OnTextCommitted(this, &SVoxelPropertyTypeSelector::OnFilterTextCommitted);

	ChildSlot
	[
		SNew(SListViewSelectorDropdownMenu<TSharedPtr<FVoxelPropertyTypeSelectorRow>>, FilterTextBox, TypeTreeView)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				FilterTextBox.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SBox)
				.HeightOverride(400.f)
				.WidthOverride(300.f)
				[
					TypeTreeView.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 4.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]
				{
					const FString ItemText = FilteredTypesCount == 1 ? " item" : " items";
					return FText::FromString(FText::AsNumber(FilteredTypesCount).ToString() + ItemText);
				})
			]
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelPropertyTypeSelector::ClearSelection() const
{
	TypeTreeView->SetSelection(nullptr, ESelectInfo::OnNavigation);
	TypeTreeView->ClearExpandedItems();
}

TSharedPtr<SWidget> SVoxelPropertyTypeSelector::GetWidgetToFocus() const
{
	return FilterTextBox;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SVoxelPropertyTypeSelector::GenerateTypeTreeRow(
	const TSharedPtr<FVoxelPropertyTypeSelectorRow> RowItem,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FLinearColor Color = RowItem->Children.Num() > 0 ? FLinearColor::White : RowItem->Type.GetColor();
	const FSlateBrush* Image = RowItem->Children.Num() > 0 ? FAppStyle::GetBrush("NoBrush") : RowItem->Type.GetInnerType().GetIcon().GetIcon();

	return
		SNew(STableRow<TSharedPtr<FVoxelPropertyTypeSelectorRow>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.f)
			[
				SNew(SImage)
				.Image(Image)
				.ColorAndOpacity(Color)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(RowItem->Name))
				.HighlightText_Lambda([this]
				{
					return SearchText;
				})
				.Font(RowItem->Children.Num() > 0 ? FAppStyle::GetFontStyle(TEXT("Kismet.TypePicker.CategoryFont")) : FAppStyle::GetFontStyle(TEXT("Kismet.TypePicker.NormalFont")))
			]
		];
}

void SVoxelPropertyTypeSelector::OnTypeSelectionChanged(
	const TSharedPtr<FVoxelPropertyTypeSelectorRow> Selection,
	const ESelectInfo::Type SelectInfo) const
{
	if (SelectInfo == ESelectInfo::OnNavigation)
	{
		if (TypeTreeView->WidgetFromItem(Selection).IsValid())
		{
			OnCloseMenu.ExecuteIfBound();
		}

		return;
	}

	if (!Selection)
	{
		return;
	}

	if (Selection->Children.Num() == 0)
	{
		OnCloseMenu.ExecuteIfBound();
		OnTypeChanged.ExecuteIfBound(Selection->Type);
		return;
	}

	TypeTreeView->SetItemExpansion(Selection, !TypeTreeView->IsItemExpanded(Selection));

	if (SelectInfo == ESelectInfo::OnMouseClick)
	{
		TypeTreeView->ClearSelection();
	}
}

void SVoxelPropertyTypeSelector::GetTypeChildren(
	const TSharedPtr<FVoxelPropertyTypeSelectorRow> PinTypeRow,
	TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>>& PinTypeRows) const
{
	PinTypeRows = PinTypeRow->Children;
}

void SVoxelPropertyTypeSelector::OnFilterTextChanged(const FText& NewText)
{
	SearchText = NewText;
	FilteredTypesList = {};

	GetChildrenMatchingSearch(NewText);
	TypeTreeView->RequestTreeRefresh();
}

void SVoxelPropertyTypeSelector::OnFilterTextCommitted(const FText& NewText, const ETextCommit::Type CommitInfo) const
{
	if (CommitInfo != ETextCommit::OnEnter)
	{
		return;
	}

	TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>> SelectedItems = TypeTreeView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		TypeTreeView->SetSelection(SelectedItems[0]);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelPropertyTypeSelector::GetChildrenMatchingSearch(const FText& InSearchText)
{
	FilteredTypesCount = 0;

	TArray<FString> FilterTerms;
	TArray<FString> SanitizedFilterTerms;

	if (InSearchText.IsEmpty())
	{
		FilteredTypesList = TypesList;
		FilteredTypesCount = TotalTypesCount;
		return;
	}

	FText::TrimPrecedingAndTrailing(InSearchText).ToString().ParseIntoArray(FilterTerms, TEXT(" "), true);

	for (int32 Index = 0; Index < FilterTerms.Num(); Index++)
	{
		FString EachString = FName::NameToDisplayString(FilterTerms[Index], false);
		EachString = EachString.Replace(TEXT(" "), TEXT(""));
		SanitizedFilterTerms.Add(EachString);
	}

	ensure(SanitizedFilterTerms.Num() == FilterTerms.Num());

	const auto SearchMatches = [&FilterTerms, &SanitizedFilterTerms](const TSharedPtr<FVoxelPropertyTypeSelectorRow>& TypeRow) -> bool
	{
		FString ItemName = TypeRow->Name;
		ItemName = ItemName.Replace(TEXT(" "), TEXT(""));

		for (int32 Index = 0; Index < FilterTerms.Num(); ++Index)
		{
			if (ItemName.Contains(FilterTerms[Index]) ||
				ItemName.Contains(SanitizedFilterTerms[Index]))
			{
				return true;
			}
		}

		return false;
	};

	const auto LookThroughList = [&](const TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>>& UnfilteredList, TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>>& OutFilteredList, auto& Lambda) -> bool
	{
		bool bReturnVal = false;
		for (const TSharedPtr<FVoxelPropertyTypeSelectorRow>& TypeRow : UnfilteredList)
		{
			const bool bMatchesItem = SearchMatches(TypeRow);
			if (TypeRow->Children.Num() == 0 ||
				bMatchesItem)
			{
				if (bMatchesItem)
				{
					OutFilteredList.Add(TypeRow);

					if (TypeRow->Children.Num() > 0 &&
						TypeTreeView.IsValid())
					{
						TypeTreeView->SetItemExpansion(TypeRow, true);
					}

					FilteredTypesCount += FMath::Max(1, TypeRow->Children.Num());

					bReturnVal = true;
				}
				continue;
			}

			TArray<TSharedPtr<FVoxelPropertyTypeSelectorRow>> ValidChildren;
			if (Lambda(TypeRow->Children, ValidChildren, Lambda))
			{
				TSharedRef<FVoxelPropertyTypeSelectorRow> NewCategory = MakeShared<FVoxelPropertyTypeSelectorRow>(TypeRow->Name, ValidChildren);
				OutFilteredList.Add(NewCategory);

				if (TypeTreeView.IsValid())
				{
					TypeTreeView->SetItemExpansion(NewCategory, true);
				}

				bReturnVal = true;
			}
		}

		return bReturnVal;
	};

	LookThroughList(TypesList, FilteredTypesList, LookThroughList);
}

void SVoxelPropertyTypeSelector::FillTypesList()
{
	TypesList = {};

	TMap<FString, TSharedPtr<FVoxelPropertyTypeSelectorRow>> Categories;

	int32 NumTypes = 0;
	for (const FVoxelPropertyType& Type : AllowedTypes.Get())
	{
		NumTypes++;

		// Only display inner type, container selection is done in dropdown
		const FVoxelPropertyType InnerType = Type.GetInnerType();

		FString TargetCategory = "Default";

		if (InnerType.Is<uint8>() &&
			InnerType.GetEnum())
		{
			TargetCategory = "Enums";
		}
		else if (InnerType.IsClass())
		{
			TargetCategory = "Classes";
		}
		else if (InnerType.IsObject())
		{
			TargetCategory = "Objects";
		}
		else if (InnerType.IsStruct())
		{
			const UScriptStruct* Struct = InnerType.GetStruct();
			if (Struct->HasMetaData("TypeCategory"))
			{
				TargetCategory = Struct->GetMetaData("TypeCategory");
			}
			else if (
				!InnerType.Is<FVector2D>() &&
				!InnerType.Is<FVector>() &&
				!InnerType.Is<FLinearColor>() &&
				!InnerType.Is<FIntPoint>() &&
				!InnerType.Is<FIntVector>() &&
				!InnerType.Is<FIntVector4>() &&
				!InnerType.Is<FQuat>() &&
				!InnerType.Is<FRotator>() &&
				!InnerType.Is<FTransform>())
			{
				TargetCategory = "Structs";
			}
		}

		if (TargetCategory.IsEmpty() ||
			TargetCategory == "Default")
		{
			TypesList.Add(MakeShared<FVoxelPropertyTypeSelectorRow>(Type));
			continue;
		}

		if (!Categories.Contains(TargetCategory))
		{
			Categories.Add(TargetCategory, MakeShared<FVoxelPropertyTypeSelectorRow>(TargetCategory));
		}

		Categories[TargetCategory]->Children.Add(MakeShared<FVoxelPropertyTypeSelectorRow>(Type));
	}

	for (const auto& It : Categories)
	{
		TypesList.Add(It.Value);
	}
	FilteredTypesList = TypesList;

	TotalTypesCount = NumTypes;
	FilteredTypesCount = NumTypes;
}