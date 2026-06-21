// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "ScopedTransaction.h"

struct VOXELCOREEDITOR_API FVoxelTransaction
{
public:
	explicit FVoxelTransaction(UObject* Object, const FString& Text = {})
		: WeakObject(Object)
		, Transaction(FText::FromString(Text), !GIsTransacting)
	{
		if (ensure(Object))
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
		if (UObject* Object = WeakObject.Resolve_Ensured())
		{
			FPropertyChangedEvent Event(ChangedProperty);
			Event.SetActiveMemberProperty(ChangedMemberProperty);
			Object->PostEditChangeProperty(Event);
		}
	}

private:
	const TVoxelObjectPtr<UObject> WeakObject;
	FProperty* ChangedProperty = nullptr;
	FProperty* ChangedMemberProperty = nullptr;
	const FScopedTransaction Transaction;
};