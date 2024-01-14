// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class VOXELCOREEDITOR_API SVoxelEditCondition : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_EVENT(FOnCheckStateChanged, OnEditConditionChanged);
		SLATE_ATTRIBUTE(ECheckBoxState, CanEdit);
	};

	static void SetupEditCondition(IDetailPropertyRow& PropertyRow, const TSharedPtr<IPropertyHandle>& Handle, const TSharedRef<SVoxelEditCondition>& EditCondition);

	void Construct(const FArguments& InArgs);

	//~ Begin SVoxelEditCondition Interface
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
	//~ End SVoxelEditCondition Interface

private:
	void OnChangeState(ECheckBoxState NewState);

	bool IsRowEnabled() const;
	ECheckBoxState GetCheckboxState() const;

private:
	FOnCheckStateChanged OnEditConditionChanged;
	TAttribute<ECheckBoxState> CanEdit;

	ECheckBoxState CachedState = ECheckBoxState::Undetermined;
};