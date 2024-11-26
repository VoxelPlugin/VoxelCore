// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "SVoxelSegmentedControl.h"

class FVoxelSegmentedEnumCustomization : public FVoxelPropertyTypeCustomizationBase
{
public:
	//~ Begin FVoxelSegmentedEnumCustomization Interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
	}
	//~ End FVoxelSegmentedEnumCustomization Interface

public:
	static TSharedRef<SWidget> CustomizeEnum(const TSharedRef<IPropertyHandle>& PropertyHandle, const IPropertyTypeCustomizationUtils& CustomizationUtils);

private:
	static TSharedRef<SVoxelSegmentedControl<uint8>> CustomizeBitmaskEnum(const TSharedRef<IPropertyHandle>& PropertyHandle, const UEnum* Enum, const int32 NumObjects);
	static TSharedRef<SVoxelSegmentedControl<uint8>> CustomizeNormalEnum(const TSharedRef<IPropertyHandle>& PropertyHandle);
};