// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class VOXELCOREEDITOR_API FVoxelDetailInterface
{
public:
	FVoxelDetailInterface(IDetailCategoryBuilder& CategoryBuilder)
		: CategoryBuilder(&CategoryBuilder)
	{
	}
	FVoxelDetailInterface(IDetailChildrenBuilder& ChildrenBuilder)
		: ChildrenBuilder(&ChildrenBuilder)
	{
	}

	bool IsCategoryBuilder() const
	{
		if (CategoryBuilder)
		{
			check(!ChildrenBuilder);
			return true;
		}
		else
		{
			check(ChildrenBuilder);
			return false;
		}
	}

	IDetailCategoryBuilder& GetCategoryBuilder() const
	{
		check(IsCategoryBuilder());
		return *CategoryBuilder;
	}
	IDetailChildrenBuilder& GetChildrenBuilder() const
	{
		check(!IsCategoryBuilder());
		return *ChildrenBuilder;
	}

public:
	IDetailPropertyRow& AddProperty(const TSharedRef<IPropertyHandle>& PropertyHandle) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddProperty(PropertyHandle);
		}
		else
		{
			return GetChildrenBuilder().AddProperty(PropertyHandle);
		}
	}
	IDetailPropertyRow* AddExternalObjects(const TArray<UObject*>& Objects, const FAddPropertyParams& Params = FAddPropertyParams()) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddExternalObjects(Objects, EPropertyLocation::Default, Params);
		}
		else
		{
			return GetChildrenBuilder().AddExternalObjects(Objects, Params);
		}
	}
	IDetailPropertyRow* AddExternalObjectProperty(const TArray<UObject*>& Objects, const FName PropertyName, const FAddPropertyParams& Params = FAddPropertyParams()) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddExternalObjectProperty(Objects, PropertyName, EPropertyLocation::Default, Params);
		}
		else
		{
			return GetChildrenBuilder().AddExternalObjectProperty(Objects, PropertyName, Params);
		}
	}
	IDetailPropertyRow* AddExternalStructure(const TSharedRef<FStructOnScope>& StructData, const FAddPropertyParams& Params = FAddPropertyParams()) const
	{
		// Both AddExternalStructure impl just call AddExternalStructureProperty with NAME_None
		return AddExternalStructureProperty(StructData, NAME_None, Params);
	}
	IDetailPropertyRow* AddExternalStructureProperty(const TSharedRef<FStructOnScope>& StructData, const FName PropertyName, const FAddPropertyParams& Params = FAddPropertyParams()) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddExternalStructureProperty(StructData, PropertyName, EPropertyLocation::Default, Params);
		}
		else
		{
			return GetChildrenBuilder().AddExternalStructureProperty(StructData, PropertyName, Params);
		}
	}
	TArray<TSharedPtr<IPropertyHandle>> AddAllExternalStructureProperties(const TSharedRef<FStructOnScope>& StructData) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddAllExternalStructureProperties(StructData, EPropertyLocation::Default, nullptr);
		}
		else
		{
			return GetChildrenBuilder().AddAllExternalStructureProperties(StructData);
		}
	}
	FDetailWidgetRow& AddCustomRow(const FText& FilterString) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddCustomRow(FilterString, false);
		}
		else
		{
			return GetChildrenBuilder().AddCustomRow(FilterString);
		}
	}
	void AddCustomBuilder(const TSharedRef<IDetailCustomNodeBuilder>& InCustomBuilder) const
	{
		if (IsCategoryBuilder())
		{
			GetCategoryBuilder().AddCustomBuilder(InCustomBuilder, false);
		}
		else
		{
			(void)GetChildrenBuilder().AddCustomBuilder(InCustomBuilder);
		}
	}
	IDetailGroup& AddGroup(const FName GroupName, const FText& LocalizedDisplayName) const
	{
		if (IsCategoryBuilder())
		{
			return GetCategoryBuilder().AddGroup(GroupName, LocalizedDisplayName, false, false);
		}
		else
		{
			return GetChildrenBuilder().AddGroup(GroupName, LocalizedDisplayName);
		}
	}

private:
	IDetailCategoryBuilder* CategoryBuilder = nullptr;
	IDetailChildrenBuilder* ChildrenBuilder = nullptr;
};