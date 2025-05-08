// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class FVoxelInstancedStructCustomization : public IPropertyTypeCustomization
{
public:
	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface

private:
	FText GetStructName() const;
	FText GetStructTooltip() const;
	const FSlateBrush* GetStructIcon() const;

	TSharedRef<SWidget> GenerateStructPicker() const;

	TSharedPtr<IPropertyHandle> StructProperty;
	FSimpleDelegate RefreshDelegate;
	TSharedPtr<SComboButton> ComboButton;
};