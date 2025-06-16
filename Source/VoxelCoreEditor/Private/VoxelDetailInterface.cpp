// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDetailInterface.h"

class FVoxelDetailsTracker
{
public:
	FVoxelDetailsViewCustomData* FindData(const TWeakPtr<const SWidget>& DetailsView)
	{
		if (!DetailsView.IsValid())
		{
			return nullptr;
		}

		ClearInactiveData();

		if (const TSharedPtr<FVoxelDetailsViewCustomData> CustomData = DetailsViewToCustomData.FindRef(DetailsView))
		{
			return CustomData.Get();
		}

		return DetailsViewToCustomData.Add(DetailsView, MakeShared<FVoxelDetailsViewCustomData>()).Get();
	}
	FVoxelDetailsViewCustomData* FindData(const IDetailsView* DetailsView)
	{
		if (!DetailsView)
		{
			return nullptr;
		}

		return FindData(DetailsView->AsWeak());
	}

private:
	void ClearInactiveData()
	{
		const float CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime < NextCheck)
		{
			return;
		}

		for (auto It = DetailsViewToCustomData.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}

		NextCheck = CurrentTime + 5.f;
	}

private:
	TMap<TWeakPtr<const SWidget>, TSharedPtr<FVoxelDetailsViewCustomData>> DetailsViewToCustomData;
	float NextCheck = -1.f;
};

FVoxelDetailsTracker* GVoxelDetailsTracker = new FVoxelDetailsTracker();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDetailInterface::FVoxelDetailInterface(IDetailCategoryBuilder& CategoryBuilder)
	: CategoryBuilder(&CategoryBuilder)
{
}

FVoxelDetailInterface::FVoxelDetailInterface(IDetailChildrenBuilder& ChildrenBuilder)
	: ChildrenBuilder(&ChildrenBuilder)
{
}

bool FVoxelDetailInterface::IsCategoryBuilder() const
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

IDetailCategoryBuilder& FVoxelDetailInterface::GetCategoryBuilder() const
{
	check(IsCategoryBuilder());
	return *CategoryBuilder;
}

IDetailChildrenBuilder& FVoxelDetailInterface::GetChildrenBuilder() const
{
	check(!IsCategoryBuilder());
	return *ChildrenBuilder;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IDetailPropertyRow& FVoxelDetailInterface::AddProperty(const TSharedRef<IPropertyHandle>& PropertyHandle) const
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

IDetailPropertyRow* FVoxelDetailInterface::AddExternalObjects(const TArray<UObject*>& Objects, const FAddPropertyParams& Params) const
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

IDetailPropertyRow* FVoxelDetailInterface::AddExternalObjectProperty(const TArray<UObject*>& Objects, const FName PropertyName, const FAddPropertyParams& Params) const
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

IDetailPropertyRow* FVoxelDetailInterface::AddExternalStructure(const TSharedRef<FStructOnScope>& StructData, const FAddPropertyParams& Params) const
{
	// Both AddExternalStructure impl just call AddExternalStructureProperty with NAME_None
	return AddExternalStructureProperty(StructData, NAME_None, Params);
}

IDetailPropertyRow* FVoxelDetailInterface::AddExternalStructure(const TSharedRef<IStructureDataProvider>& StructData, const FAddPropertyParams& Params) const
{
	// Both AddExternalStructure impl just call AddExternalStructureProperty with NAME_None
	return AddExternalStructureProperty(StructData, NAME_None, Params);
}

IDetailPropertyRow* FVoxelDetailInterface::AddExternalStructureProperty(const TSharedRef<FStructOnScope>& StructData, const FName PropertyName, const FAddPropertyParams& Params) const
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

IDetailPropertyRow* FVoxelDetailInterface::AddExternalStructureProperty(const TSharedRef<IStructureDataProvider>& StructData, const FName PropertyName, const FAddPropertyParams& Params) const
{
	if (IsCategoryBuilder())
	{
		return GetCategoryBuilder().AddExternalStructureProperty(StructData, PropertyName, EPropertyLocation::Default, Params);
	}
	else
	{
		// FStructurePropertyNode::GetInstancesNum will crash
		ensure(!StructData->IsPropertyIndirection());
		return GetChildrenBuilder().AddExternalStructureProperty(StructData, PropertyName, Params);
	}
}

TArray<TSharedPtr<IPropertyHandle>> FVoxelDetailInterface::AddAllExternalStructureProperties(const TSharedRef<FStructOnScope>& StructData) const
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

FDetailWidgetRow& FVoxelDetailInterface::AddCustomRow(const FText& FilterString) const
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

void FVoxelDetailInterface::AddCustomBuilder(const TSharedRef<IDetailCustomNodeBuilder>& InCustomBuilder) const
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

IDetailGroup& FVoxelDetailInterface::AddGroup(const FName GroupName, const FText& LocalizedDisplayName) const
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

bool FVoxelDetailInterface::IsPropertyEditingEnabled() const
{
	if (IsCategoryBuilder())
	{
		return GetCategoryBuilder().GetParentLayout().GetPropertyUtilities()->IsPropertyEditingEnabled();
	}
	else
	{
		return GetChildrenBuilder().GetParentCategory().GetParentLayout().GetPropertyUtilities()->IsPropertyEditingEnabled();
	}
}

UE_506_SWITCH(IDetailsView*, TSharedPtr<IDetailsView>) FVoxelDetailInterface::GetDetailsView() const
{
#if VOXEL_ENGINE_VERSION >= 506
	if (IsCategoryBuilder())
	{
		return GetCategoryBuilder().GetParentLayout().GetDetailsViewSharedPtr();
	}
	else
	{
		return GetChildrenBuilder().GetParentCategory().GetParentLayout().GetDetailsViewSharedPtr();
	}
#else
	if (IsCategoryBuilder())
	{
		return GetCategoryBuilder().GetParentLayout().GetDetailsView();
	}
	else
	{
		return GetChildrenBuilder().GetParentCategory().GetParentLayout().GetDetailsView();
	}
#endif
}

FVoxelDetailsViewCustomData* FVoxelDetailInterface::GetCustomData() const
{
	const UE_506_SWITCH(IDetailsView*, TSharedPtr<IDetailsView>) DetailsView = GetDetailsView();
	if (!DetailsView)
	{
		return nullptr;
	}

	return GVoxelDetailsTracker->FindData(DetailsView);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDetailsViewCustomData::FVoxelDetailsViewCustomData()
{
	ValueColumnWidth = MakeAttributeLambda([this]
	{
		return ValueColumnWidthValue;
	});
	ToolTipColumnWidth = MakeAttributeLambda([this]
	{
		return 1.f - ValueColumnWidthValue;
	});

	HoveredSplitterIndex = MakeAttributeLambda([this]
	{
		return HoveredSplitterIndexValue;
	});

	OnValueColumnResized = MakeLambdaDelegate([this](const float NewWidth)
	{
		ensure(NewWidth >= 0.f && NewWidth <= 1.f);
		ValueColumnWidthValue = FMath::Clamp(NewWidth, 0.f, 1.f);
	});

	OnSplitterHandleHovered = MakeLambdaDelegate([this](const int32 NewIndex)
	{
		HoveredSplitterIndexValue = NewIndex;
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TAttribute<float> FVoxelDetailsViewCustomData::GetValueColumnWidth() const
{
	return ValueColumnWidth;
}

TAttribute<float> FVoxelDetailsViewCustomData::GetToolTipColumnWidth() const
{
	return ToolTipColumnWidth;
}

TAttribute<int32> FVoxelDetailsViewCustomData::GetHoveredSplitterIndex() const
{
	return HoveredSplitterIndex;
}

const SSplitter::FOnSlotResized& FVoxelDetailsViewCustomData::GetOnValueColumnResized() const
{
	return OnValueColumnResized;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelDetailsViewCustomData::HasMetadata(const FName Name) const
{
	return Metadata.Contains(Name);
}

const FString* FVoxelDetailsViewCustomData::GetMetadata(const FName Name) const
{
	if (const FString* Value = Metadata.Find(Name))
	{
		return Value;
	}

	return nullptr;
}

void FVoxelDetailsViewCustomData::SetMetadata(const FName Key, const FString& Value)
{
	Metadata.FindOrAdd(Key) = Value;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDetailsViewCustomData* FVoxelDetailsViewCustomData::GetCustomData(const IDetailsView* DetailsView)
{
	if (!DetailsView)
	{
		return nullptr;
	}

	return GetCustomData(DetailsView->AsWeak());
}

FVoxelDetailsViewCustomData* FVoxelDetailsViewCustomData::GetCustomData(const TSharedRef<const SWidget>& DetailsView)
{
	return GetCustomData(MakeWeakPtr(DetailsView));
}

FVoxelDetailsViewCustomData* FVoxelDetailsViewCustomData::GetCustomData(const TSharedPtr<const SWidget>& DetailsView)
{
	return GetCustomData(MakeWeakPtr(DetailsView));
}

FVoxelDetailsViewCustomData* FVoxelDetailsViewCustomData::GetCustomData(const TWeakPtr<const SWidget>& DetailsView)
{
	return GVoxelDetailsTracker->FindData(DetailsView);
}