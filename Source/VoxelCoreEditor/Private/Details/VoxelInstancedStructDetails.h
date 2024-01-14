// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

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
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	FText GetDisplayValueString() const;
	const FSlateBrush* GetDisplayValueIcon() const;
	TSharedRef<SWidget> GenerateStructPicker();
	void OnStructPicked(const UScriptStruct* InStruct);

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** The base struct that we're allowing to be picked (controlled by the "BaseStruct" meta-data) */
	UScriptStruct* BaseScriptStruct = nullptr;

	TSharedPtr<class SComboButton> ComboButton;
};

/**
 * Node builder for FVoxelInstancedStruct children.
 * Expects property handle holding FVoxelInstancedStruct as input.
 * Can be used in a implementation of a IPropertyTypeCustomization CustomizeChildren() to display editable FVoxelInstancedStruct contents.
 * OnChildRowAdded() is called right after each property is added, which allows the property row to be customizable.
 */
class FVoxelInstancedStructDataDetails : public IDetailCustomNodeBuilder, public FSelfRegisteringEditorUndoClient, public TSharedFromThis<FVoxelInstancedStructDataDetails>
{
public:
	FVoxelInstancedStructDataDetails(TSharedPtr<IPropertyHandle> InStructProperty);

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	/** Pre/Post change notifications for struct value changes */
	void OnStructValuePreChange();
	void OnStructValuePostChange();
	void OnStructHandlePostChange();

	/** Sync the current state of the editable struct instance to/from the source instance(s) */
	void SyncEditableInstanceFromSource(bool* OutStructMismatch = nullptr);
	void SyncEditableInstanceToSource(bool* OutStructMismatch = nullptr);

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** Struct instance that is being edited; this is a copy of the source struct data to avoid lifetime issues when the underlying source is updated/deleted */
	TSharedPtr<FStructOnScope> StructInstanceData;

	/** Delegate that can be used to refresh the child rows of the current struct (eg, when changing struct type) */
	FSimpleDelegate OnRegenerateChildren;

	/** The last time that SyncEditableInstanceFromSource was called, in FPlatformTime::Seconds() */
	double LastSyncEditableInstanceFromSourceSeconds = 0.0;

	/** True if we're currently handling a StructValuePostChange */
	bool bIsHandlingStructValuePostChange = false;
};