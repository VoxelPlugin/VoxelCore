// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "SVoxelDetailWidgets.h"
#include "VoxelEditorMinimal.h"

void SVoxelDetailText::Construct(const FArguments& Args)
{
	ChildSlot
	[
		SNew(STextBlock)
		.Font(Args._Font.IsSet() ? Args._Font : FVoxelEditorUtilities::Font())
		.Text(Args._Text)
		.HighlightText(Args._HighlightText)
		.ColorAndOpacity(Args._ColorAndOpacity)
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelDetailButton::Construct(const FArguments& Args)
{
	ChildSlot
	[
		SNew(SButton)
		.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(Args._OnClicked)
		[
			SNew(SVoxelDetailText)
			.Text(Args._Text)
		]
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelAlwaysEnabledWidget::Construct(const FArguments& Args)
{
	ChildSlot
	[
		Args._Content.Widget
	];
}

int32 SVoxelAlwaysEnabledWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	if (!ShouldBeEnabled(bParentEnabled))
	{
		for (TSharedPtr<SWidget> Widget = ConstCast(this)->AsShared(); Widget; Widget = Widget->GetParentWidget())
		{
			if (!Widget->IsEnabled())
			{
				Widget->SetEnabled(true);
			}
		}
	}

	return SCompoundWidget::OnPaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
}