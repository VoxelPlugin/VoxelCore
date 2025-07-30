// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "IStructureDataProvider.h"

struct FVoxelDetailsViewCustomData;

class VOXELCOREEDITOR_API FVoxelDetailInterface
{
public:
	FVoxelDetailInterface(IDetailCategoryBuilder& CategoryBuilder);
	FVoxelDetailInterface(IDetailChildrenBuilder& ChildrenBuilder);

	bool IsCategoryBuilder() const;

	IDetailCategoryBuilder& GetCategoryBuilder() const;
	IDetailChildrenBuilder& GetChildrenBuilder() const;

public:
	IDetailPropertyRow& AddProperty(const TSharedRef<IPropertyHandle>& PropertyHandle) const;
	IDetailPropertyRow* AddExternalObjects(const TArray<UObject*>& Objects, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	IDetailPropertyRow* AddExternalObjectProperty(const TArray<UObject*>& Objects, const FName PropertyName, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	IDetailPropertyRow* AddExternalStructure(const TSharedRef<FStructOnScope>& StructData, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	IDetailPropertyRow* AddExternalStructure(const TSharedRef<IStructureDataProvider>& StructData, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	IDetailPropertyRow* AddExternalStructureProperty(const TSharedRef<FStructOnScope>& StructData, const FName PropertyName, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	IDetailPropertyRow* AddExternalStructureProperty(const TSharedRef<IStructureDataProvider>& StructData, const FName PropertyName, const FAddPropertyParams& Params = FAddPropertyParams()) const;
	TArray<TSharedPtr<IPropertyHandle>> AddAllExternalStructureProperties(const TSharedRef<FStructOnScope>& StructData) const;
	FDetailWidgetRow& AddCustomRow(const FText& FilterString) const;
	void AddCustomBuilder(const TSharedRef<IDetailCustomNodeBuilder>& InCustomBuilder) const;
	IDetailGroup& AddGroup(const FName GroupName, const FText& LocalizedDisplayName) const;
	bool IsPropertyEditingEnabled() const;
	UE_506_SWITCH(IDetailsView*, TSharedPtr<IDetailsView>) GetDetailsView() const;

	FVoxelDetailsViewCustomData* GetCustomData() const;

private:
	IDetailCategoryBuilder* CategoryBuilder = nullptr;
	IDetailChildrenBuilder* ChildrenBuilder = nullptr;
};

struct VOXELCOREEDITOR_API FVoxelDetailsViewCustomData
{
	FVoxelDetailsViewCustomData();

public:
	TAttribute<float> GetValueColumnWidth() const;
	TAttribute<float> GetToolTipColumnWidth() const;
	TAttribute<int32> GetHoveredSplitterIndex() const;

	const SSplitter::FOnSlotResized& GetOnValueColumnResized() const;

public:
	bool HasMetadata(FName Name) const;
	const FString* GetMetadata(FName Name) const;
	void SetMetadata(FName Key, const FString& Value = "");

public:
	static FVoxelDetailsViewCustomData* GetCustomData(const IDetailsView* DetailsView);
	static FVoxelDetailsViewCustomData* GetCustomData(const TSharedRef<const SWidget>& DetailsView);
	static FVoxelDetailsViewCustomData* GetCustomData(const TSharedPtr<const SWidget>& DetailsView);
	static FVoxelDetailsViewCustomData* GetCustomData(const TWeakPtr<const SWidget>& DetailsView);

private:
	float ValueColumnWidthValue = 0.6f;
	int32 HoveredSplitterIndexValue = -1;

	TAttribute<float> ValueColumnWidth;
	TAttribute<float> ToolTipColumnWidth;
	TAttribute<int32> HoveredSplitterIndex;

	SSplitter::FOnSlotResized OnValueColumnResized;
	SSplitter::FOnHandleHovered OnSplitterHandleHovered;

	TVoxelMap<FName, FString> Metadata;
};