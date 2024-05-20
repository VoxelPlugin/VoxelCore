// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelFalloff.h"
#include "Styling/SlateStyleRegistry.h"

class FVoxelFalloffCustomization : public IPropertyTypeCustomization
{
public:
	virtual void CustomizeHeader(const TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		const TSharedRef<SSegmentedControl<EVoxelFalloff>> ButtonsList =
			SNew(SSegmentedControl<EVoxelFalloff>)
			.OnValueChanged_Lambda([WeakHandle = MakeWeakPtr(StructPropertyHandle)](EVoxelFalloff NewValue)
			{
				const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
				if (!ensure(Handle))
				{
					return;
				}

				const TSharedRef<IPropertyHandle> FalloffHandle = Handle->GetChildHandleStatic(FVoxelFalloffWrapper, Falloff);
				FalloffHandle->SetValue(uint8(NewValue));
			})
			.Value_Lambda([WeakHandle = MakeWeakPtr(StructPropertyHandle)]
			{
				const TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
				if (!ensure(Handle))
				{
					return EVoxelFalloff::Linear;
				}

				const TSharedRef<IPropertyHandle> FalloffHandle = Handle->GetChildHandleStatic(FVoxelFalloffWrapper, Falloff);
				return FVoxelEditorUtilities::GetEnumPropertyValue<EVoxelFalloff>(FalloffHandle);
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
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			ButtonsList
		];
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
	}
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelFalloffWrapper, FVoxelFalloffCustomization);