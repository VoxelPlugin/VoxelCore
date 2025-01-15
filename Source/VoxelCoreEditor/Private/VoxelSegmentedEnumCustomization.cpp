// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSegmentedEnumCustomization.h"

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	ForEachObjectOfClass<UEnum>([&](const UEnum& Enum)
	{
		if (!Enum.HasMetaData(TEXT("VoxelSegmentedEnum")))
		{
			return;
		}

		FVoxelEditorUtilities::RegisterEnumLayout(&Enum, FOnGetPropertyTypeCustomizationInstance::CreateLambda([]
		{
			return MakeShared<TVoxelPropertyTypeCustomizationWrapper<FVoxelSegmentedEnumCustomization>>();
		}), nullptr);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSegmentedEnumCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		CustomizeEnum(PropertyHandle, CustomizationUtils)
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> FVoxelSegmentedEnumCustomization::CustomizeEnum(const TSharedRef<IPropertyHandle>& PropertyHandle, const IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!ensure(PropertyHandle->IsValidHandle()))
	{
		return SNullWidget::NullWidget;
	}

	const UEnum* Enum = INLINE_LAMBDA -> UEnum*
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(PropertyHandle->GetProperty()))
		{
			return EnumProperty->GetEnum();
		}

		if (CastField<FIntProperty>(PropertyHandle->GetProperty()))
		{
			if (PropertyHandle->HasMetaData("BitmaskEnum"))
			{
				return UClass::TryFindTypeSlow<UEnum>(PropertyHandle->GetMetaData("BitmaskEnum"));
			}
		}

		return nullptr;
	};

	if (!ensure(Enum))
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SVoxelSegmentedControl<uint8>> Widget;
	if (PropertyHandle->HasMetaData("Bitmask"))
	{
		Widget = CustomizeBitmaskEnum(
			PropertyHandle,
			Enum,
			CustomizationUtils.GetPropertyUtilities()->GetSelectedObjects().Num());
	}
	else
	{
		Widget = CustomizeNormalEnum(PropertyHandle);
	}

	for (int32 Index = 0; Index < Enum->NumEnums() - 1; Index++)
	{
		if (Enum->HasMetaData(TEXT("Hidden"), Index))
		{
			continue;
		}

		if (Enum->HasMetaData(TEXT("Icon"), Index))
		{
			const ISlateStyle* StyleSet = &FAppStyle::Get();
			if (Enum->HasMetaData(TEXT("StyleSet"), Index))
			{
				if (const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(FName(Enum->GetMetaData(TEXT("StyleSet"), Index))))
				{
					StyleSet = Style;
				}
			}

			const FName BrushName = FName(Enum->GetMetaData(TEXT("Icon"), Index));
			Widget->AddSlot(Enum->GetValueByIndex(Index))
			.Icon(StyleSet->GetBrush(BrushName))
			.ToolTip(Enum->GetToolTipTextByIndex(Index));
			continue;
		}

		FSlateColor Color = FSlateColor::UseForeground();
		if (Enum->HasMetaData(TEXT("Color"), Index))
		{
			const FString ColorName = Enum->GetMetaData(TEXT("Color"), Index).ToLower();
			const int32 StyleColorValue = StaticEnumFast<EStyleColor>()->GetValueByNameString(ColorName, EGetByNameFlags::None);
			if (StyleColorValue != -1)
			{
				Color = USlateThemeManager::Get().GetColor(EStyleColor(StyleColorValue));
			}
			else if (GColorList.IsValidColorName(*ColorName))
			{
				Color = GColorList.GetFColorByName(*ColorName);
			}
		}

		FSlateFontInfo Font = FVoxelEditorUtilities::Font();
		if (Enum->HasMetaData(TEXT("Font")))
		{
			Font = FAppStyle::GetFontStyle(*Enum->GetMetaData(TEXT("Font"), Index));
		}

		Widget->AddSlot(Enum->GetValueByIndex(Index))
		.ToolTip(Enum->GetToolTipTextByIndex(Index))
		[
			SNew(SVoxelDetailText)
			.Text(Enum->GetDisplayNameTextByIndex(Index))
			.ColorAndOpacity(Color)
			.Font(Font)
		];
	}

	return Widget.ToSharedRef();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SVoxelSegmentedControl<uint8>> FVoxelSegmentedEnumCustomization::CustomizeBitmaskEnum(const TSharedRef<IPropertyHandle>& PropertyHandle, const UEnum* Enum, const int32 NumObjects)
{
	const auto GetValues = [Enum, NumObjects, WeakHandle = MakeWeakPtr(PropertyHandle)]() -> TMap<uint8, ECheckBoxState>
	{
		VOXEL_SCOPE_COUNTER("SVoxelSegmentedControl::BitMaskValues");

		const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
		if (!Handle ||
			!Handle->IsValidHandle())
		{
			return {};
		}

		TMap<uint8, ECheckBoxState> Result;
		TMap<uint8, int32> Occurrences;
		
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; Index++)
		{
			if (Enum->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}

			const uint8 EnumValue = Enum->GetValueByIndex(Index);
			Occurrences.Add(EnumValue, 0);
		}
		Handle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			if (!ensure(RawData))
			{
				return true;
			}

			const int64 Value = *static_cast<int64*>(RawData);
			for (auto& It : Occurrences)
			{
				if ((Value & It.Key) == It.Key)
				{
					It.Value++;
				}
			}
			return true;
		});

		for (const auto& It : Occurrences)
		{
			ECheckBoxState State = ECheckBoxState::Undetermined;
			if (It.Value == NumObjects)
			{
				State = ECheckBoxState::Checked;
			}
			else if (It.Value == 0)
			{
				State = ECheckBoxState::Unchecked;
			}

			Result.Add(It.Key, State);
		}

		return Result;
	};

	TSharedRef<SVoxelSegmentedControl<uint8>> Widget =
		SNew(SVoxelSegmentedControl<uint8>)
		.SupportsMultiSelection(true)
		.OnValuesChanged_Lambda([WeakHandle = MakeWeakPtr(PropertyHandle)](const TArray<uint8> AddedValues, const TArray<uint8> RemovedValues)
		{
			const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
			if (!ensure(Handle) ||
				!Handle->IsValidHandle())
			{
				return;
			}

			TArray<FString> Values;
			Handle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
			{
				if (!ensure(RawData))
				{
					return true;
				}

				int64 Value = *static_cast<int64*>(RawData);
				for (const uint8 AddedValue : AddedValues)
				{
					Value |= AddedValue;
				}
				for (const uint8 RemovedValue : RemovedValues)
				{
					Value &= ~RemovedValue;
				}

				Values.Add(FVoxelUtilities::PropertyToText_Direct(*Handle->GetProperty(), static_cast<const void*>(&Value), nullptr));
				return true;
			});

			Handle->SetPerObjectValues(Values);
		})
		.Values(GetValues());

	PropertyHandle->SetOnPropertyValueChanged(MakeWeakPtrDelegate(Widget, [GetValues, WeakWidget = MakeWeakPtr(Widget)]
	{
		const TSharedPtr<SVoxelSegmentedControl<uint8>> PinnedWidget = WeakWidget.Pin();
		if (!PinnedWidget)
		{
			return;
		}

		PinnedWidget->SetValues(GetValues(), true);
	}));
	return Widget;
}

TSharedRef<SVoxelSegmentedControl<uint8>> FVoxelSegmentedEnumCustomization::CustomizeNormalEnum(const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	const auto GetValues = [WeakHandle = MakeWeakPtr(PropertyHandle)]() -> TMap<uint8, ECheckBoxState>
	{
		VOXEL_SCOPE_COUNTER("SVoxelSegmentedControl::EnumValues");

		const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
		if (!ensure(Handle) ||
			!Handle->IsValidHandle())
		{
			return {};
		}

		TMap<uint8, ECheckBoxState> Result;
		Handle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
		{
			if (!ensure(RawData))
			{
				return true;
			}

			const uint8 Value = *static_cast<uint8*>(RawData);
			Result.Add(Value, ECheckBoxState::Checked);
			return true;
		});

		if (Result.Num() > 1)
		{
			for (auto& It : Result)
			{
				It.Value = ECheckBoxState::Undetermined;
			}
		}

		return Result;
	};

	TSharedRef<SVoxelSegmentedControl<uint8>> Widget =
		SNew(SVoxelSegmentedControl<uint8>)
		.OnValueChanged_Lambda([WeakHandle = MakeWeakPtr(PropertyHandle)](const uint8 NewValue)
		{
			const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
			if (!ensure(Handle) ||
				!Handle->IsValidHandle())
			{
				return;
			}

			Handle->SetValue(NewValue);
		})
		.Values(GetValues());

	PropertyHandle->SetOnPropertyValueChanged(MakeWeakPtrDelegate(Widget, [GetValues, WeakWidget = MakeWeakPtr(Widget)]
	{
		const TSharedPtr<SVoxelSegmentedControl<uint8>> PinnedWidget = WeakWidget.Pin();
		if (!PinnedWidget)
		{
			return;
		}

		PinnedWidget->SetValues(GetValues(), true);
	}));
	return Widget;
}