// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

struct FVoxelPropertyType;

class VOXELCOREEDITOR_API FVoxelPropertyCustomizationUtilities
{
public:
	static TSharedPtr<FVoxelInstancedStructDetailsWrapper> CreateValueCustomization(
		const TSharedRef<IPropertyHandle>& PropertyHandle,
		const FVoxelDetailInterface& DetailInterface,
		const FSimpleDelegate& RefreshDelegate,
		const TMap<FName, FString>& MetaData,
		TFunctionRef<void(FDetailWidgetRow&, const TSharedRef<SWidget>&)> SetupRow,
		const FAddPropertyParams& Params,
		const TAttribute<bool>& IsEnabled = true);

	static float GetValueWidgetWidthByType(
		const TSharedPtr<IPropertyHandle>& PropertyHandle,
		const FVoxelPropertyType& Type);

private:
	static TSharedPtr<SWidget> AddArrayItemOptions(
		const TSharedRef<IPropertyHandle>& PropertyHandle,
		const TSharedPtr<SWidget>& ValueWidget);
};