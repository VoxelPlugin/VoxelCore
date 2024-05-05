// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelPropertyValue.h"
#include "VoxelPropertyCustomizationUtilities.h"

class FVoxelPropertyValueCustomization
	: public IPropertyTypeCustomization
	, public FVoxelTicker
{
public:
	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(
		const TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		TypeHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Type);
		CachedType = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPropertyType>(TypeHandle);
		RefreshDelegate = FVoxelEditorUtilities::MakeRefreshDelegate(this, CustomizationUtils);

		if (!CachedType.IsValid())
		{
			HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SVoxelDetailText)
				.Text(INVTEXT("Invalid type"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
	}

	virtual void CustomizeChildren(
		const TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		if (const TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle())
		{
			if (ParentHandle->AsArray())
			{
				for (const auto& It : *ParentHandle->GetInstanceMetaDataMap())
				{
					PropertyHandle->SetInstanceMetaData(It.Key, It.Value);
				}
			}
		}

		Wrapper = FVoxelPropertyCustomizationUtilities::CreateValueCustomization(
			PropertyHandle,
			ChildBuilder,
			FVoxelEditorUtilities::MakeRefreshDelegate(this, CustomizationUtils),
			{},
			[&](FDetailWidgetRow& Row, const TSharedRef<SWidget>& ValueWidget)
			{
				const FVoxelPropertyType Type = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPropertyType>(TypeHandle);
				const float Width = FVoxelPropertyCustomizationUtilities::GetValueWidgetWidthByType(PropertyHandle, Type);

				Row
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MinDesiredWidth(Width)
				.MaxDesiredWidth(Width)
				[
					ValueWidget
				];
			},
			// Used to load/save expansion state
			FAddPropertyParams().UniqueId("FVoxelPropertyValueCustomization"));
	}
	//~ End IPropertyTypeCustomization Interface

	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		if (!TypeHandle ||
			TypeHandle->GetNumPerObjectValues() == 0)
		{
			return;
		}

		const FVoxelPropertyType Type = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPropertyType>(TypeHandle);
		if (Type != CachedType)
		{
			RefreshDelegate.ExecuteIfBound();
			TypeHandle = nullptr;
		}
	}
	//~ End FVoxelTicker Interface

private:
	TSharedPtr<FVoxelInstancedStructDetailsWrapper> Wrapper;

	TSharedPtr<IPropertyHandle> TypeHandle;
	FVoxelPropertyType CachedType;
	FSimpleDelegate RefreshDelegate;
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelPropertyValue, FVoxelPropertyValueCustomization);
DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelPropertyTerminalValue, FVoxelPropertyValueCustomization);