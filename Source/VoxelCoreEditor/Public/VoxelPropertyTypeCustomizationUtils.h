// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class VOXELCOREEDITOR_API FVoxelPropertyTypeCustomizationUtils : public IPropertyTypeCustomizationUtils
{
public:
	explicit FVoxelPropertyTypeCustomizationUtils(const IPropertyTypeCustomizationUtils& CustomizationUtils)
		: ThumbnailPool(CustomizationUtils.GetThumbnailPool())
		, PropertyUtilities(CustomizationUtils.GetPropertyUtilities())
	{
	}

	//~ Begin IPropertyTypeCustomizationUtils Interface
	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override
	{
		return ThumbnailPool;
	}
	virtual TSharedPtr<IPropertyUtilities> GetPropertyUtilities() const override
	{
		return PropertyUtilities;
	}
	//~ End IPropertyTypeCustomizationUtils Interface

private:
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};