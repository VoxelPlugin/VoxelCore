// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class FVoxelInstancedStructNodeBuilder
	: public IDetailCustomNodeBuilder
	, public TSharedFromThis<FVoxelInstancedStructNodeBuilder>
{
public:
	const TSharedRef<IPropertyHandle> StructProperty;

	explicit FVoxelInstancedStructNodeBuilder(const TSharedRef<IPropertyHandle>& StructProperty)
		: StructProperty(StructProperty)
	{
	}

	void Initialize();

public:
	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate NewOnRebuildChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:
	TVoxelArray<UScriptStruct*> GetStructs() const;

	FSimpleDelegate OnRebuildChildren;
	TVoxelArray<UScriptStruct*> LastStructs;
	bool bDisableObjectCountLimit = false;
};