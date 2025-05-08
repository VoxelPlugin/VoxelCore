// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class VOXELCOREEDITOR_API FVoxelCategoryBuilder
{
public:
	const FName BaseNameForExpansionState;

	explicit FVoxelCategoryBuilder(const FName BaseNameForExpansionState)
		: BaseNameForExpansionState(BaseNameForExpansionState)
	{
	}

	using FAddProperty = TFunction<void(const FVoxelDetailInterface& DetailInterface)>;

	void AddProperty(
		const FString& Category,
		const FAddProperty& AddProperty);

	void Apply(IDetailLayoutBuilder& DetailLayout) const;
	void Apply(IDetailChildrenBuilder& ChildrenBuilder) const;
	void Apply(const FVoxelDetailInterface& DetailInterface) const;
	void ApplyFlat(const FVoxelDetailInterface& DetailInterface) const;

private:
	struct FCategory
	{
		FString Name;
		TVoxelArray<FAddProperty> AddProperties;
		TMap<FName, TSharedPtr<FCategory>> NameToChild;

		void Apply(
			const FString& CategoryPath,
			const FVoxelDetailInterface& DetailInterface);
	};
	class FCustomNodeBuilder : public IDetailCustomNodeBuilder
	{
	public:
		FString CategoryPath;
		TSharedPtr<FCategory> Category;

		//~ Begin IDetailCustomNodeBuilder Interface
		virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
		virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
		virtual FName GetName() const override;
		virtual bool InitiallyCollapsed() const override;
		//~ End IDetailCustomNodeBuilder Interface
	};

private:
	const TSharedRef<FCategory> RootCategory = MakeShared<FCategory>();
};