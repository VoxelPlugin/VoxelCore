// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "ScopedTransaction.h"

struct VOXELCOREEDITOR_API FVoxelTransaction
{
public:
	explicit FVoxelTransaction(const TWeakObjectPtr<UObject>& Object, const FString& Text = {})
		: Object(Object)
		, Transaction(FText::FromString(Text), !GIsTransacting)
	{
		if (ensure(Object.IsValid()))
		{
			Object->PreEditChange(nullptr);
		}
	}
	explicit FVoxelTransaction(UObject& Object, const FString& Text = {})
		: FVoxelTransaction(&Object, Text)
	{
	}
	explicit FVoxelTransaction(const UEdGraphPin* Pin, const FString& Text = {})
		: FVoxelTransaction(Pin ? Pin->GetOwningNode() : nullptr, Text)
	{
	}
	void SetProperty(FProperty& Property)
	{
		ChangedProperty = &Property;
		ChangedMemberProperty = &Property;
	}
	void SetMemberProperty(FProperty& Property)
	{
		ChangedMemberProperty = &Property;
	}
	~FVoxelTransaction()
	{
		if (ensure(Object.IsValid()))
		{
			FPropertyChangedEvent Event(ChangedProperty);
			Event.SetActiveMemberProperty(ChangedMemberProperty);
			Object->PostEditChangeProperty(Event);
		}
	}

private:
	const TWeakObjectPtr<UObject> Object;
	FProperty* ChangedProperty = nullptr;
	FProperty* ChangedMemberProperty = nullptr;
	const FScopedTransaction Transaction;
};