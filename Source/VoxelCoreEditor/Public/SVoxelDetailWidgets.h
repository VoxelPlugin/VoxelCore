// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class VOXELCOREEDITOR_API SVoxelDetailText : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(FText, Text);
		SLATE_ATTRIBUTE(FText, HighlightText);
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity);
		SLATE_ATTRIBUTE(FSlateFontInfo, Font);
	};

	void Construct(const FArguments& Args);
};

class VOXELCOREEDITOR_API SVoxelDetailButton : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(FText, Text);
		SLATE_EVENT(FOnClicked, OnClicked);
	};

	void Construct(const FArguments& Args);
};

class VOXELCOREEDITOR_API SVoxelAlwaysEnabledWidget : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_DEFAULT_SLOT(FArguments, Content)
	};

	void Construct(const FArguments& Args);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
};