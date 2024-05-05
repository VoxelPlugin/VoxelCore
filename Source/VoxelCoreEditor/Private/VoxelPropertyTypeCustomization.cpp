// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "SVoxelPropertyTypeComboBox.h"

class FVoxelPropertyTypeCustomization : public IPropertyTypeCustomization
{
public:
	virtual void CustomizeHeader(const TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		const TArray<TWeakObjectPtr<UObject>> SelectedObjects = CustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();

		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SVoxelPropertyTypeComboBox)
			.AllowedTypes_Lambda([]() -> TVoxelSet<FVoxelPropertyType>
			{
				// TODO
				return {};
			})
			.OnTypeChanged_Lambda([=](const FVoxelPropertyType& Type)
			{
				FVoxelEditorUtilities::SetStructPropertyValue(PropertyHandle, Type);
			})
			.CurrentType_Lambda([WeakHandle = MakeWeakPtr(PropertyHandle)]() -> FVoxelPropertyType
			{
				const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
				if (!Handle)
				{
					return {};
				}

				return FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPropertyType>(Handle);
			})
		];
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
	}
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelPropertyType, FVoxelPropertyTypeCustomization);