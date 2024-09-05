// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelPrimitiveComponentSettings.h"

class FVoxelPrimitiveComponentSettingsCustomization : public FVoxelPropertyTypeCustomizationBase
{
public:
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		if (PropertyHandle->HasMetaData(STATIC_FNAME("ShowOnlyInnerProperties")))
		{
			return;
		}

		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget()
		];
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		uint32 NumChildren = 0;
		ensure(PropertyHandle->GetNumChildren(NumChildren) == FPropertyAccess::Success);

		TMap<FName, IDetailGroup*> SubcategoryNameToGroup;
		for (uint32 Index = 0; Index < NumChildren; Index++)
		{
			const TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(Index);
			if (!ensure(ChildHandle) ||
				!ensure(ChildHandle->IsValidHandle()))
			{
				continue;
			}

			FName GroupName = ChildHandle->GetDefaultCategoryName();
			if (GroupName.IsNone() ||
				GroupName == STATIC_FNAME("Default"))
			{
				ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
				continue;
			}

			IDetailGroup* Group = SubcategoryNameToGroup.FindRef(GroupName);
			if (!Group)
			{
				Group = SubcategoryNameToGroup.Add(GroupName, &ChildBuilder.AddGroup(GroupName, ChildHandle->GetDefaultCategoryText()));
			}

			Group->AddPropertyRow(ChildHandle.ToSharedRef());
		}
	}
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelPrimitiveComponentSettings, FVoxelPrimitiveComponentSettingsCustomization);