// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelSegmentedControl.h"

VOXEL_INITIALIZE_STYLE(VoxelSegmentedControlStyle)
{
	FSegmentedControlStyle SegmentedControlStyle = FAppStyle::GetWidgetStyle<FSegmentedControlStyle>("SegmentedControl");

	const FName UndeterminedImage = FName(RootToCoreContentDir("Sequencer/Section/SectionHeaderSelectedSectionOverlay", TEXT(".png")));

	const FCheckBoxStyle SingleCheckBoxStyle =
		FCheckBoxStyle(SegmentedControlStyle.ControlStyle)
		.SetUndeterminedImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::Hover, CoreStyleConstants::InputFocusRadius, FStyleColors::Primary, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both))
		.SetUndeterminedHoveredImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::Hover2, CoreStyleConstants::InputFocusRadius, FStyleColors::PrimaryHover, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both))
		.SetUndeterminedPressedImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::Hover, CoreStyleConstants::InputFocusRadius, FStyleColors::PrimaryPress, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both));

	Set("VoxelSingleSegmentedControl",
		FSegmentedControlStyle(SegmentedControlStyle)
		.SetControlStyle(SingleCheckBoxStyle)
		.SetFirstControlStyle(SingleCheckBoxStyle)
		.SetLastControlStyle(SingleCheckBoxStyle));

	const FCheckBoxStyle MultiCheckBoxStyle =
		FCheckBoxStyle(SegmentedControlStyle.ControlStyle)
		.SetCheckedImage(FSlateRoundedBoxBrush(FStyleColors::Primary, CoreStyleConstants::InputFocusRadius))
		.SetCheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryHover, CoreStyleConstants::InputFocusRadius))
		.SetCheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryPress, CoreStyleConstants::InputFocusRadius))
		.SetUndeterminedImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::Primary, CoreStyleConstants::InputFocusRadius, FStyleColors::Primary, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both))
		.SetUndeterminedHoveredImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::PrimaryHover, CoreStyleConstants::InputFocusRadius, FStyleColors::PrimaryHover, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both))
		.SetUndeterminedPressedImage(FSlateRoundedBoxBrush(UndeterminedImage, FStyleColors::PrimaryPress, CoreStyleConstants::InputFocusRadius, FStyleColors::PrimaryPress, 0.f, CoreStyleConstants::Icon16x16, ESlateBrushTileType::Both));

	Set("VoxelMultiSegmentedControl",
		FSegmentedControlStyle(SegmentedControlStyle)
		.SetControlStyle(MultiCheckBoxStyle)
		.SetFirstControlStyle(MultiCheckBoxStyle)
		.SetLastControlStyle(MultiCheckBoxStyle)
		.SetBackgroundBrush(*FAppStyle::GetBrush("NoBorder"))
		.SetUniformPadding(FMargin(4.f, 8.f)));
}