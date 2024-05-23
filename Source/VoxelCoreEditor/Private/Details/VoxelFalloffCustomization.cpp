// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelFalloff.h"
#include "Styling/SlateStyleRegistry.h"

VOXEL_CUSTOMIZE_ENUM_HEADER(EVoxelFalloff)(const TSharedRef<IPropertyHandle> EnumPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedRef<SSegmentedControl<EVoxelFalloff>> ButtonsList =
		SNew(SSegmentedControl<EVoxelFalloff>)
		.OnValueChanged_Lambda([WeakHandle = MakeWeakPtr(EnumPropertyHandle)](EVoxelFalloff NewValue)
		{
			const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
			if (!ensure(Handle))
			{
				return;
			}

			Handle->SetValue(uint8(NewValue));
		})
		.Value_Lambda([WeakHandle = MakeWeakPtr(EnumPropertyHandle)]
		{
			const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
			if (!ensure(Handle))
			{
				return EVoxelFalloff::Linear;
			}

			return FVoxelEditorUtilities::GetEnumPropertyValue<EVoxelFalloff>(Handle);
		});

	const UEnum* Enum = StaticEnumFast<EVoxelFalloff>();
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; Index++)
	{
		if (Enum->HasMetaData(TEXT("Hidden"), Index))
		{
			continue;
		}

		const ISlateStyle* StyleSet = &FAppStyle::Get();
		if (Enum->HasMetaData(TEXT("StyleSet"), Index))
		{
			if (const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(FName(Enum->GetMetaData(TEXT("StyleSet"), Index))))
			{
				StyleSet = Style;
			}
		}

		const FName BrushName = FName(Enum->GetMetaData(TEXT("Icon"), Index));
		ButtonsList->AddSlot(static_cast<EVoxelFalloff>(Enum->GetValueByIndex(Index)))
		.Icon(StyleSet->GetBrush(BrushName))
		.ToolTip(Enum->GetToolTipTextByIndex(Index));
	}

	HeaderRow
	.NameContent()
	[
		EnumPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		ButtonsList
	];
}