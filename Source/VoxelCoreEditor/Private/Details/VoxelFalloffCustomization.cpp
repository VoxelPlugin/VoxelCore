// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelFalloff.h"

class FVoxelFalloffCustomization : public FVoxelPropertyTypeCustomizationBase
{
public:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SAssignNew(HeaderValueBox, SBox)
		];
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		{
			IDetailPropertyRow& Row = ChildBuilder.AddProperty(PropertyHandle->GetChildHandleStatic(FVoxelFalloff, Type));
			TSharedPtr<SWidget> DummyNameWidget;
			TSharedPtr<SWidget> ValueWidget;
			Row.GetDefaultWidgets(DummyNameWidget, ValueWidget);
			Row.Visibility(EVisibility::Collapsed);

			HeaderValueBox->SetContent(ValueWidget.ToSharedRef());
		}
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandleStatic(FVoxelFalloff, Amount));
	}

private:
	TSharedPtr<SBox> HeaderValueBox;
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelFalloff, FVoxelFalloffCustomization);