// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPropertyCustomizationUtilities.h"
#include "VoxelPropertyValue.h"

TSharedPtr<FVoxelInstancedStructDetailsWrapper> FVoxelPropertyCustomizationUtilities::CreateValueCustomization(
	const TSharedRef<IPropertyHandle>& PropertyHandle,
	const FVoxelDetailInterface& DetailInterface,
	const FSimpleDelegate& RefreshDelegate,
	const TMap<FName, FString>& MetaData,
	const TFunctionRef<void(FDetailWidgetRow&, const TSharedRef<SWidget>&)> SetupRow,
	const FAddPropertyParams& Params,
	const TAttribute<bool>& IsEnabled)
{
	ensure(RefreshDelegate.IsBound());

	const auto ApplyMetaData = [&](const TSharedRef<IPropertyHandle>& ChildHandle)
	{
		if (const FProperty* MetaDataProperty = PropertyHandle->GetMetaDataProperty())
		{
			if (const TMap<FName, FString>* MetaDataMap = MetaDataProperty->GetMetaDataMap())
			{
				for (const auto& It : *MetaDataMap)
				{
					ChildHandle->SetInstanceMetaData(It.Key, It.Value);
				}
			}
		}

		if (const TMap<FName, FString>* MetaDataMap = PropertyHandle->GetInstanceMetaDataMap())
		{
			for (const auto& It : *MetaDataMap)
			{
				ChildHandle->SetInstanceMetaData(It.Key, It.Value);
			}
		}

		for (const auto& It : MetaData)
		{
			ChildHandle->SetInstanceMetaData(It.Key, It.Value);
		}
	};

	const TSharedRef<IPropertyHandle> TypeHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Type);
	const FVoxelPropertyType Type = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPropertyType>(TypeHandle);
	if (!Type.IsValid())
	{
		return nullptr;
	}

	if (Type.GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		const TSharedRef<IPropertyHandle> ArrayHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Array);

		ApplyMetaData(ArrayHandle);

		IDetailPropertyRow& Row = DetailInterface.AddProperty(ArrayHandle);

		// Disable child rows
		Row.EditCondition(IsEnabled, {});

		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		Row.GetDefaultWidgets(NameWidget, ValueWidget, true);
		Row.ShowPropertyButtons(false);
		Row.ShouldAutoExpand();

		SetupRow(Row.CustomWidget(true), ValueWidget.ToSharedRef());

		return nullptr;
	}
	ensure(Type.GetContainerType() == EVoxelPropertyContainerType::None);

	if (Type.IsStruct())
	{
		const TSharedRef<IPropertyHandle> StructHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Struct);
		ApplyMetaData(StructHandle);
		const TSharedPtr<FVoxelInstancedStructDetailsWrapper> Wrapper = FVoxelInstancedStructDetailsWrapper::Make(StructHandle);
		if (!ensure(Wrapper))
		{
			return nullptr;
		}

		IDetailPropertyRow* Row = Wrapper->AddExternalStructure(DetailInterface, Params);
		if (!ensure(Row))
		{
			return nullptr;
		}

		// Disable child rows
		Row->EditCondition(IsEnabled, {});

		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		Row->GetDefaultWidgets(NameWidget, ValueWidget, true);

		SetupRow(Row->CustomWidget(true), AddArrayItemOptions(PropertyHandle, ValueWidget).ToSharedRef());

		return Wrapper;
	}

	TSharedPtr<IPropertyHandle> ValueHandle;
	const TSharedPtr<SWidget> ValueWidget = INLINE_LAMBDA -> TSharedPtr<SWidget>
	{
		switch (Type.GetInternalType())
		{
		default:
		{
			ensure(false);
			return nullptr;
		}
		case EVoxelPropertyInternalType::Bool:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, bBool);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Float:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Float);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Double:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Double);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Int32:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Int32);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Int64:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Int64);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Name:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Name);
			ApplyMetaData(ValueHandle.ToSharedRef());
			return ValueHandle->CreatePropertyValueWidget();
		}
		case EVoxelPropertyInternalType::Byte:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Byte);
			ApplyMetaData(ValueHandle.ToSharedRef());

			const UEnum* Enum = Type.GetEnum();
			if (!Enum)
			{
				return ValueHandle->CreatePropertyValueWidget();
			}

			uint8 Byte = 0;
			switch (ValueHandle->GetValue(Byte))
			{
			default:
			{
				ensure(false);
				return nullptr;
			}
			case FPropertyAccess::MultipleValues:
			{
				return SNew(SVoxelDetailText).Text(INVTEXT("Multiple Values"));
			}
			case FPropertyAccess::Fail:
			{
				ensure(false);
				return nullptr;
			}
			case FPropertyAccess::Success: break;
			}

			return
				SNew(SBox)
				.MinDesiredWidth(125.f)
				[
					SNew(SVoxelDetailComboBox<uint8>)
					.RefreshDelegate(RefreshDelegate)
					.Options_Lambda([=]
					{
						TArray<uint8> Enumerators;
						for (int32 Index = 0; Index < Enum->NumEnums() - 1; Index++)
						{
							if (Enum->HasMetaData(TEXT("Hidden"), Index) ||
								Enum->HasMetaData(TEXT("Spacer"), Index))
							{
								continue;
							}

							Enumerators.Add(Enum->GetValueByIndex(Index));
						}

						return Enumerators;
					})
					.CurrentOption(Byte)
					.OptionText(MakeLambdaDelegate([=](const uint8 Value)
					{
						FString EnumDisplayName = Enum->GetDisplayNameTextByValue(Value).ToString();
						if (EnumDisplayName.Len() == 0)
						{
							return Enum->GetNameStringByValue(Value);
						}

						return EnumDisplayName;
					}))
					.OnSelection_Lambda([ValueHandle](const uint8 NewValue)
					{
						ensure(ValueHandle->SetValue(uint8(NewValue)) == FPropertyAccess::Success);
					})
				];
		}
		case EVoxelPropertyInternalType::Class:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Class);
			ApplyMetaData(ValueHandle.ToSharedRef());

			ValueHandle->GetProperty()->SetMetaData("AllowedClasses", Type.GetBaseClass()->GetPathName());
			const TSharedRef<SWidget> CustomValueWidget = ValueHandle->CreatePropertyValueWidget();
			ValueHandle->GetProperty()->RemoveMetaData("AllowedClasses");

			return CustomValueWidget;
		}
		case EVoxelPropertyInternalType::Object:
		{
			ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelPropertyValue, Object);
			ApplyMetaData(ValueHandle.ToSharedRef());

			ValueHandle->GetProperty()->SetMetaData("AllowedClasses", Type.GetObjectClass()->GetPathName());
			const TSharedRef<SWidget> CustomValueWidget = ValueHandle->CreatePropertyValueWidget();
			ValueHandle->GetProperty()->RemoveMetaData("AllowedClasses");

			return CustomValueWidget;
		}
		}
	};

	if (!ValueWidget)
	{
		return nullptr;
	}

	FDetailWidgetRow& Row = DetailInterface.AddProperty(ValueHandle.ToSharedRef()).CustomWidget();

	SetupRow(
		Row,
		AddArrayItemOptions(PropertyHandle, ValueWidget).ToSharedRef());

	return nullptr;
}

float FVoxelPropertyCustomizationUtilities::GetValueWidgetWidthByType(
	const TSharedPtr<IPropertyHandle>& PropertyHandle,
	const FVoxelPropertyType& Type)
{
	const bool bIsArrayItem =
		PropertyHandle &&
		PropertyHandle->GetParentHandle() &&
		CastField<FArrayProperty>(PropertyHandle->GetParentHandle()->GetProperty());

	const float ExtendByArray = bIsArrayItem ? 32.f : 0.f;

	if (!Type.IsValid())
	{
		return FDetailWidgetRow::DefaultValueMaxWidth + ExtendByArray;
	}

	if (Type.Is<FVector2D>() ||
		Type.Is<FIntPoint>() ||
		Type.Is<FVoxelFloatRange>() ||
		Type.Is<FVoxelInt32Range>())
	{
		return FDetailWidgetRow::DefaultValueMaxWidth * 2.f + ExtendByArray;
	}

	if (Type.Is<FVector>() ||
		Type.Is<FIntVector>() ||
		Type.Is<FQuat>())
	{
		return FDetailWidgetRow::DefaultValueMaxWidth * 3.f + ExtendByArray;
	}

	if (Type.Is<FVector4>() ||
		Type.Is<FIntVector4>())
	{
		return FDetailWidgetRow::DefaultValueMaxWidth * 4.f + ExtendByArray;
	}

	if (Type.IsObject())
	{
		return FDetailWidgetRow::DefaultValueMaxWidth * 2.f + ExtendByArray;
	}

	return FDetailWidgetRow::DefaultValueMaxWidth + ExtendByArray;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> FVoxelPropertyCustomizationUtilities::AddArrayItemOptions(
	const TSharedRef<IPropertyHandle>& PropertyHandle,
	const TSharedPtr<SWidget>& ValueWidget)
{
	if (!PropertyHandle->GetParentHandle() ||
		!CastField<FArrayProperty>(PropertyHandle->GetParentHandle()->GetProperty()))
	{
		return ValueWidget;
	}

	const FExecuteAction InsertAction = MakeWeakPtrDelegate(PropertyHandle, [PropertyHandle]
	{
		const TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
		check(ArrayHandle.IsValid());

		const int32 Index = PropertyHandle->GetIndexInArray();
		ArrayHandle->Insert(Index);
	});
	const FExecuteAction DeleteAction = MakeWeakPtrDelegate(PropertyHandle, [PropertyHandle]
	{
		const TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
		check(ArrayHandle.IsValid());

		const int32 Index = PropertyHandle->GetIndexInArray();
		ArrayHandle->DeleteItem(Index);
	});
	const FExecuteAction DuplicateAction = MakeWeakPtrDelegate(PropertyHandle, [PropertyHandle]
	{
		const TSharedPtr<IPropertyHandleArray> ArrayHandle = PropertyHandle->GetParentHandle()->AsArray();
		check(ArrayHandle.IsValid());

		const int32 Index = PropertyHandle->GetIndexInArray();
		ArrayHandle->DuplicateItem(Index);
	});

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			ValueWidget.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(4.0f, 1.0f, 0.0f, 1.0f)
		[
			PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(InsertAction, DeleteAction, DuplicateAction)
		];
}