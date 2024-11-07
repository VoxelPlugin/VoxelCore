// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "IStructureDataProvider.h"

class IPropertyHandle;
class IDetailPropertyRow;
class FStructOnScope;
struct FVoxelInstancedStruct;

/**
 * Type customization for FVoxelInstancedStruct.
 */
class FVoxelInstancedStructDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	void OnObjectsReinstanced(const TMap<UObject*, UObject*>& ObjectMap);

	FText GetDisplayValueString() const;
	FText GetTooltipText() const;
	const FSlateBrush* GetDisplayValueIcon() const;
	TSharedRef<SWidget> GenerateStructPicker();
	void OnStructPicked(const UScriptStruct* InStruct);

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** The base struct that we're allowing to be picked (controlled by the "BaseStruct" meta-data) */
	UScriptStruct* BaseScriptStruct = nullptr;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<IPropertyUtilities> PropUtils;
	
	FDelegateHandle OnObjectsReinstancedHandle;
};

class FVoxelInstancedStructProvider : public IStructureDataProvider
{
public:
	const TSharedPtr<IPropertyHandle> StructProperty;

	explicit FVoxelInstancedStructProvider(const TSharedPtr<IPropertyHandle>& StructProperty)
		: StructProperty(StructProperty)
	{
	}

	virtual bool IsValid() const override;
	virtual const UStruct* GetBaseStructure() const override;
	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override;

	virtual bool IsPropertyIndirection() const override
	{
		return true;
	}

	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const override;

private:
	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const;
};

/**
 * Node builder for FVoxelInstancedStruct children.
 * Expects property handle holding FVoxelInstancedStruct as input.
 * Can be used in a implementation of a IPropertyTypeCustomization CustomizeChildren() to display editable FVoxelInstancedStruct contents.
 * OnChildRowAdded() is called right after each property is added, which allows the property row to be customizable.
 */
class FVoxelInstancedStructDataDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FVoxelInstancedStructDataDetails>
{
public:
	FVoxelInstancedStructDataDetails(
		const TSharedPtr<IPropertyHandle>& InStructProperty,
		const TSharedRef<FVoxelInstancedStructProvider>& StructProvider);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:
	/** Pre/Post change notifications for struct value changes */
	void OnStructHandlePostChange();

	/** Returns type of the instanced struct for each instance/object being edited. */
	TArray<TWeakObjectPtr<const UStruct>> GetInstanceTypes() const;

	/** Cached instance types, used to invalidate the layout when types change. */
	TArray<TWeakObjectPtr<const UStruct>> CachedInstanceTypes;
	
	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** Struct provider for the structs. */
	TSharedPtr<FVoxelInstancedStructProvider> StructProvider;

	/** Delegate that can be used to refresh the child rows of the current struct (eg, when changing struct type) */
	FSimpleDelegate OnRegenerateChildren;

	/** True if we're currently handling a StructValuePostChange */
	bool bIsHandlingStructValuePostChange = false;

	bool bIsInitialGeneration = true;
	
protected:
	void OnStructLayoutChanges();
};