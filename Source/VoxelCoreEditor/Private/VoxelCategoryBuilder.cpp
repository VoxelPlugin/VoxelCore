// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelCategoryBuilder.h"

void FVoxelCategoryBuilder::AddProperty(
	const FString& Category,
	const FAddProperty& AddProperty)
{
	TSharedPtr<FCategory> CategoryRef = RootCategory;
	if (!Category.IsEmpty())
	{
		for (const FString& SubCategory : FVoxelUtilities::ParseCategory(Category))
		{
			TSharedPtr<FCategory>& SubCategoryRef = CategoryRef->NameToChild.FindOrAdd(FName(SubCategory));
			if (!SubCategoryRef)
			{
				SubCategoryRef = MakeShared<FCategory>();
				SubCategoryRef->Name = SubCategory;
			}
			CategoryRef = SubCategoryRef;
		}
	}
	CategoryRef->AddProperties.Add(AddProperty);
}

void FVoxelCategoryBuilder::Apply(IDetailLayoutBuilder& DetailLayout) const
{
	VOXEL_FUNCTION_COUNTER();

	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Default");
		for (const FAddProperty& AddProperty : RootCategory->AddProperties)
		{
			AddProperty(Category);
		}
	}

	for (const auto& It : RootCategory->NameToChild)
	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory(It.Key);
		It.Value->Apply(
			(BaseNameForExpansionState.IsNone() ? "FVoxelCategoryBuilder" : BaseNameForExpansionState.ToString()) +
			"." +
			It.Key.ToString() +
			".",
			Category);
	}
}

void FVoxelCategoryBuilder::Apply(IDetailChildrenBuilder& ChildrenBuilder) const
{
	VOXEL_FUNCTION_COUNTER();

	for (const FAddProperty& AddProperty : RootCategory->AddProperties)
	{
		AddProperty(ChildrenBuilder);
	}

	for (const auto& It : RootCategory->NameToChild)
	{
		const TSharedRef<FCustomNodeBuilder> CustomBuilder = MakeShared<FCustomNodeBuilder>();

		CustomBuilder->CategoryPath =
			(BaseNameForExpansionState.IsNone() ? "FVoxelCategoryBuilder" : BaseNameForExpansionState.ToString()) +
			"." +
			It.Key.ToString();

		CustomBuilder->Category = It.Value;

		ChildrenBuilder.AddCustomBuilder(CustomBuilder);
	}
}

void FVoxelCategoryBuilder::Apply(const FVoxelDetailInterface& DetailInterface) const
{
	VOXEL_FUNCTION_COUNTER();

	RootCategory->Apply(
		BaseNameForExpansionState.ToString() + ".",
		DetailInterface);
}

void FVoxelCategoryBuilder::ApplyFlat(const FVoxelDetailInterface& DetailInterface) const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<FAddProperty> AddProperties;

	const TFunction<void(FCategory&)> Visit = [&](const FCategory& Category)
	{
		AddProperties.Append(Category.AddProperties);

		for (const auto& It : Category.NameToChild)
		{
			Visit(*It.Value);
		}
	};
	Visit(*RootCategory);

	for (const FAddProperty& AddProperty : AddProperties)
	{
		AddProperty(DetailInterface);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelCategoryBuilder::FCategory::Apply(
	const FString& CategoryPath,
	const FVoxelDetailInterface& DetailInterface)
{
	for (const FAddProperty& AddProperty : AddProperties)
	{
		AddProperty(DetailInterface);
	}

	for (const auto& It : NameToChild)
	{
		const TSharedRef<FCustomNodeBuilder> CustomBuilder = MakeShared<FCustomNodeBuilder>();
		CustomBuilder->CategoryPath = CategoryPath + "|" + It.Key.ToString();
		CustomBuilder->Category = It.Value;
		DetailInterface.AddCustomBuilder(CustomBuilder);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelCategoryBuilder::FCustomNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	NodeRow.NameContent()
	[
		SNew(SVoxelDetailText)
		.Text(FText::FromString(Category->Name))
	];
}

void FVoxelCategoryBuilder::FCustomNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	Category->Apply(CategoryPath, ChildrenBuilder);
}

FName FVoxelCategoryBuilder::FCustomNodeBuilder::GetName() const
{
	return FName(CategoryPath);
}

bool FVoxelCategoryBuilder::FCustomNodeBuilder::InitiallyCollapsed() const
{
	// Match categories' behavior
	return false;
}