// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelEditCondition.h"

void SVoxelEditCondition::SetupEditCondition(IDetailPropertyRow& PropertyRow, const TSharedPtr<IPropertyHandle>& Handle, const TSharedRef<SVoxelEditCondition>& EditCondition)
{
	FDetailWidgetRow& Row = PropertyRow.CustomWidget(true);

	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row, true);

	Row
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			EditCondition
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1.f)
		[
			NameWidget.ToSharedRef()
		]
	]
	.ValueContent()
	[
		ValueWidget.ToSharedRef()
	];

	PropertyRow.EditCondition(MakeAttributeSP(&EditCondition.Get(), &SVoxelEditCondition::IsRowEnabled), nullptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelEditCondition::Construct(const FArguments& InArgs)
{
	OnEditConditionChanged = InArgs._OnEditConditionChanged;
	CanEdit = InArgs._CanEdit;
	CachedState = CanEdit.Get();

	ChildSlot
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this, &SVoxelEditCondition::OnChangeState)
		.IsChecked(this, &SVoxelEditCondition::GetCheckboxState)
	];

	SetEnabled(true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelEditCondition::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (IsParentValid() &&
		GetParentWidget()->GetType() == SHorizontalBox::StaticWidgetClass().GetWidgetType())
	{
		TSharedPtr<SWidget> Parent = GetParentWidget();
		Parent->SetEnabled(true);
		if (Parent->IsParentValid() &&
			Parent->GetParentWidget()->GetType() == SBox::StaticWidgetClass().GetWidgetType())
		{
			Parent = Parent->GetParentWidget();
			Parent->SetEnabled(true);
			if (Parent->IsParentValid() &&
				Parent->GetParentWidget()->GetType() == SHorizontalBox::StaticWidgetClass().GetWidgetType())
			{
				Parent = Parent->GetParentWidget();
				Parent->SetEnabled(true);
			}
		}
	}

	if (!IsEnabled())
	{
		SetEnabled(true);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelEditCondition::OnChangeState(const ECheckBoxState NewState)
{
	OnEditConditionChanged.ExecuteIfBound(NewState);
	CachedState = CanEdit.Get();
	SetEnabled(true);
}

bool SVoxelEditCondition::IsRowEnabled() const
{
	return CachedState == ECheckBoxState::Checked;
}

ECheckBoxState SVoxelEditCondition::GetCheckboxState() const
{
	return CachedState;
}