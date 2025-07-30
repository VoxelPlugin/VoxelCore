// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInstancedStructNodeBuilder.h"
#include "VoxelInstancedStructDataProvider.h"

void FVoxelInstancedStructNodeBuilder::Initialize()
{
	VOXEL_FUNCTION_COUNTER();

	StructProperty->SetOnPropertyValueChanged(MakeWeakPtrDelegate(this, [this]
	{
		if (LastStructs != GetStructs())
		{
			OnRebuildChildren.ExecuteIfBound();
		}
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelArray<UScriptStruct*> FVoxelInstancedStructNodeBuilder::GetStructs() const
{
	TVoxelArray<UScriptStruct*> Result;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Result.Add(InstancedStruct.GetScriptStruct());
	});
	return Result;
}

void FVoxelInstancedStructNodeBuilder::SetOnRebuildChildren(const FSimpleDelegate NewOnRebuildChildren)
{
	OnRebuildChildren = NewOnRebuildChildren;
}

void FVoxelInstancedStructNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}

void FVoxelInstancedStructNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	VOXEL_FUNCTION_COUNTER();

	LastStructs = GetStructs();

	if (StructProperty->GetNumPerObjectValues() > 500 &&
		!bDisableObjectCountLimit)
	{
		ChildBuilder.AddCustomRow(INVTEXT("Expand"))
		.WholeRowContent()
		[
			SNew(SVoxelDetailButton)
			.Text(FText::FromString(FString::Printf(TEXT("Expand %d structs"), StructProperty->GetNumPerObjectValues())))
			.OnClicked_Lambda([this]
			{
				bDisableObjectCountLimit = true;
				OnRebuildChildren.ExecuteIfBound();
				return FReply::Handled();
			})
		];

		return;
	}

	const auto ApplyMetaData = [&](IPropertyHandle& Handle)
	{
		if (!Handle.IsValidHandle())
		{
			return;
		}

		if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
		{
			if (const TMap<FName, FString>* MetaDataMap = MetaDataProperty->GetMetaDataMap())
			{
				for (const auto& It : *MetaDataMap)
				{
					Handle.SetInstanceMetaData(It.Key, It.Value);
				}
			}
		}

		if (const TMap<FName, FString>* MetaDataMap = StructProperty->GetInstanceMetaDataMap())
		{
			for (const auto& It : *MetaDataMap)
			{
				Handle.SetInstanceMetaData(It.Key, It.Value);
			}
		}
	};

	const TSharedRef<FVoxelInstancedStructDataProvider> StructProvider = MakeShared<FVoxelInstancedStructDataProvider>(StructProperty);

	const FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const UStruct* BaseStruct = StructProvider->GetBaseStructure();

	if (BaseStruct &&
		PropertyModule.IsCustomizedStruct(BaseStruct, FCustomPropertyTypeLayoutMap()))
	{
		// Use the struct name instead of the fully-qualified property name
		const FText Label = BaseStruct->GetDisplayNameText();
		const FName PropertyName = StructProperty->GetProperty()->GetFName();

		// If the struct has a property customization, then we'll route through AddChildStructure, as it supports
		// IPropertyTypeCustomization. The other branch is mostly kept as-is for legacy support purposes.
		IDetailPropertyRow* PropertyRow = ChildBuilder.AddChildStructure(
			StructProperty,
			StructProvider,
			PropertyName,
			Label);

		// Expansion state is not properly persisted for these structures, so let's expand it by default for now
		if (PropertyRow)
		{
			PropertyRow->ShouldAutoExpand(true);

			if (const TSharedPtr<IPropertyHandle> Handle = PropertyRow->GetPropertyHandle())
			{
				ApplyMetaData(*Handle);
			}
		}
	}
	else
	{
		const TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(StructProvider);

		INLINE_LAMBDA
		{
			uint32 NumChildren = 0;
			StructProperty->GetNumChildren(NumChildren);

			if (!ensure(NumChildren > 0))
			{
				return;
			}

			const TSharedPtr<IPropertyHandle> Handle = StructProperty->GetChildHandle(NumChildren - 1);
			if (!ensure(Handle))
			{
				return;
			}

			ApplyMetaData(*Handle);
		};

		for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildProperties)
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

void FVoxelInstancedStructNodeBuilder::Tick(float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();

	if (LastStructs != GetStructs())
	{
		OnRebuildChildren.ExecuteIfBound();
	}
}

FName FVoxelInstancedStructNodeBuilder::GetName() const
{
	return "VoxelInstancedStructNodeBuilder";
}