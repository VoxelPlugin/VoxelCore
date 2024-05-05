// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelPropertyTypeComboBox.h"
#include "SVoxelPropertyTypeSelector.h"

void SVoxelPropertyTypeComboBox::Construct(const FArguments& InArgs)
{
	bAllowArray = InArgs._AllowArray;
	AllowedTypes = InArgs._AllowedTypes;
	CurrentType = InArgs._CurrentType;
	ReadOnly = InArgs._ReadOnly;

	OnTypeChanged = InArgs._OnTypeChanged;
	ensure(OnTypeChanged.IsBound());

	SAssignNew(MainIcon, SImage);

	SAssignNew(MainTextBlock, STextBlock)
	.Font(FVoxelEditorUtilities::Font())
	.ColorAndOpacity(FSlateColor::UseForeground());

	UpdateType(CurrentType.Get());

	const bool bShowContainerSelection = bAllowArray;

	TSharedPtr<SHorizontalBox> SelectorBox;
	ChildSlot
	[
		SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]
		{
			return ReadOnly.Get() ? 1 : 0;
		})
		+ SWidgetSwitcher::Slot()
		.Padding(InArgs._DetailsWindow ? FMargin(0.f) : FMargin(-6.f, 0.f,0.f,0.f))
		[
			SAssignNew(SelectorBox, SHorizontalBox)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(InArgs._DetailsWindow ? 125.f : FOptionalSize())
				[
					SAssignNew(TypeComboButton, SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "ComboButton")
					.OnGetMenuContent(this, &SVoxelPropertyTypeComboBox::GetMenuContent)
					.ContentPadding(0)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						.Clipping(EWidgetClipping::ClipToBoundsAlways)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.Padding(0.f, 0.f, 2.f, 0.f)
						.AutoWidth()
						[
							MainIcon.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.Padding(2.f, 0.f, 0.f, 0.f)
						.AutoWidth()
						[
							MainTextBlock.ToSharedRef()
						]
					]
				]
			]
		]
		+ SWidgetSwitcher::Slot()
		[
			SNew(SHorizontalBox)
			.Clipping(EWidgetClipping::OnDemand)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(MakeAttributeLambda(MakeWeakPtrLambda(this, [this, bShowContainerSelection]
			{
				return FMargin(2.f, bShowContainerSelection ? 4.f : 3.f);
			})))
			.AutoWidth()
			[
				MainIcon.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(2.f, 0.f, 0.f, 0.f)
			.AutoWidth()
			[
				MainTextBlock.ToSharedRef()
			]
		]
	];

	if (!bShowContainerSelection)
	{
		return;
	}

	SelectorBox->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	.Padding(2.f)
	[
		SAssignNew(ContainerTypeComboButton, SComboButton)
		.ComboButtonStyle(FAppStyle::Get(),"BlueprintEditor.CompactVariableTypeSelector")
		.MenuPlacement(MenuPlacement_ComboBoxRight)
		.OnGetMenuContent(this, &SVoxelPropertyTypeComboBox::GetContainerTypeMenuContent)
		.ContentPadding(0.f)
		.ButtonContent()
		[
			MainIcon.ToSharedRef()
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelPropertyTypeComboBox::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!CurrentType.IsBound())
	{
		return;
	}

	const FVoxelPropertyType NewType = CurrentType.Get();
	if (NewType == CachedType)
	{
		return;
	}

	UpdateType(NewType);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SVoxelPropertyTypeComboBox::GetMenuContent()
{
	if (MenuContent)
	{
		TypeSelector->ClearSelection();

		return MenuContent.ToSharedRef();
	}

	SAssignNew(MenuContent, SMenuOwner)
	[
		SAssignNew(TypeSelector, SVoxelPropertyTypeSelector)
		.AllowedTypes(AllowedTypes)
		.OnTypeChanged_Lambda([this](FVoxelPropertyType NewType)
		{
			NewType.SetContainerType(CachedType.GetContainerType());

			OnTypeChanged.ExecuteIfBound(NewType);
			UpdateType(NewType);
		})
		.OnCloseMenu_Lambda([this]
		{
			MenuContent->CloseSummonedMenus();
			TypeComboButton->SetIsOpen(false);
		})
	];

	TypeComboButton->SetMenuContentWidgetToFocus(TypeSelector->GetWidgetToFocus());

	return MenuContent.ToSharedRef();
}

TSharedRef<SWidget> SVoxelPropertyTypeComboBox::GetContainerTypeMenuContent()
{
	if (ContainerTypeMenuContent)
	{
		return ContainerTypeMenuContent.ToSharedRef();
	}

	FMenuBuilder MenuBuilder(false, nullptr);

	const auto AddMenu = [&](const EVoxelPropertyContainerType ContainerType, const FText& Label, const FSlateBrush* Brush)
	{
		FUIAction Action;
		Action.ExecuteAction.BindSP(this, &SVoxelPropertyTypeComboBox::OnContainerTypeSelectionChanged, ContainerType);

		MenuBuilder.AddMenuEntry(Action,
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(Brush)
				.ColorAndOpacity(CachedType.GetColor())
			]
			+ SHorizontalBox::Slot()
			.Padding(4.0f,2.0f)
			[
				SNew(STextBlock)
				.Text(Label)
			]);
	};

	AddMenu(EVoxelPropertyContainerType::None, INVTEXT("Uniform"), FAppStyle::Get().GetBrush("Kismet.VariableList.TypeIcon"));

	if (bAllowArray)
	{
		AddMenu(EVoxelPropertyContainerType::Array, INVTEXT("Array"), FAppStyle::Get().GetBrush("Kismet.VariableList.ArrayTypeIcon"));
	}

	SAssignNew(ContainerTypeMenuContent, SMenuOwner)
	[
		MenuBuilder.MakeWidget()
	];

	return ContainerTypeMenuContent.ToSharedRef();
}

void SVoxelPropertyTypeComboBox::OnContainerTypeSelectionChanged(const EVoxelPropertyContainerType ContainerType)
{
	FVoxelPropertyType NewType = CachedType;
	NewType.SetContainerType(ContainerType);

	OnTypeChanged.ExecuteIfBound(NewType);
	UpdateType(NewType);

	ContainerTypeMenuContent->CloseSummonedMenus();
	ContainerTypeComboButton->SetIsOpen(false);
}

void SVoxelPropertyTypeComboBox::UpdateType(const FVoxelPropertyType& NewType)
{
	CachedType = NewType;

	MainIcon->SetImage(CachedType.GetIcon().GetIcon());
	MainIcon->SetColorAndOpacity(CachedType.GetInnerType().GetColor());

	MainTextBlock->SetText(FText::FromString(CachedType.GetInnerType().ToString()));
}