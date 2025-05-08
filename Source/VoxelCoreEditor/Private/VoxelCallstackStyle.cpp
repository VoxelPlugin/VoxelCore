// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"

VOXEL_INITIALIZE_STYLE(VoxelCallstack)
{
	const FTextBlockStyle DefaultTextStyle = FTextBlockStyle(FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetFont(DEFAULT_FONT("Mono", 9))
		.SetShadowOffset(FVector2f::ZeroVector)
		.SetColorAndOpacity(FSlateColor::UseForeground());
	const FTextBlockStyle SubduedTextStyle = FTextBlockStyle(FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetFont(DEFAULT_FONT("Mono", 9))
		.SetShadowOffset(FVector2f::ZeroVector)
		.SetColorAndOpacity(FSlateColor::UseSubduedForeground());
	const FTextBlockStyle MarkedTextStyle = FTextBlockStyle(FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
		.SetFont(DEFAULT_FONT("Mono", 9))
		.SetShadowOffset(FVector2f::ZeroVector)
		.SetColorAndOpacity(FStyleColors::Error);

	const FButtonStyle DefaultButtonStyle =
		FButtonStyle()
		.SetNormal(CORE_BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0.f, 0.f, 0.f, 3.f / 16.f), FSlateColor::UseForeground()))
		.SetPressed(FSlateNoResource())
		.SetHovered(CORE_BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0.f, 0.f, 0.f, 3.f / 16.0f), FSlateColor::UseForeground()));
	const FButtonStyle SubduedButtonStyle =
		FButtonStyle()
		.SetNormal(CORE_BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0.f, 0.f, 0.f, 3.f / 16.f), FSlateColor::UseSubduedForeground()))
		.SetPressed(FSlateNoResource())
	.SetHovered(CORE_BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0.f, 0.f, 0.f, 3.f / 16.0f), FSlateColor::UseSubduedForeground()));
	const FButtonStyle MarkedButtonStyle =
		FButtonStyle()
		.SetNormal(CORE_BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0.f, 0.f, 0.f, 3.f / 16.f), FStyleColors::Error))
		.SetPressed(FSlateNoResource())
		.SetHovered(CORE_BORDER_BRUSH("Old/HyperlinkUnderline", FMargin(0.f, 0.f, 0.f, 3.f / 16.0f), FStyleColors::Error));

	Set("Callstack.Default", DefaultTextStyle);
	Set("Callstack.Subdued", SubduedTextStyle);
	Set("Callstack.Marked", MarkedTextStyle);

	Set("Callstack.Default.Hyperlink",
		FHyperlinkStyle(FAppStyle::GetWidgetStyle<FHyperlinkStyle>("Hyperlink"))
		.SetTextStyle(DefaultTextStyle)
		.SetUnderlineStyle(DefaultButtonStyle));
	Set("Callstack.Subdued.Hyperlink",
		FHyperlinkStyle(FAppStyle::GetWidgetStyle<FHyperlinkStyle>("Hyperlink"))
		.SetTextStyle(SubduedTextStyle)
		.SetUnderlineStyle(SubduedButtonStyle));
	Set("Callstack.Marked.Hyperlink",
		FHyperlinkStyle(FAppStyle::GetWidgetStyle<FHyperlinkStyle>("Hyperlink"))
		.SetTextStyle(MarkedTextStyle)
		.SetUnderlineStyle(MarkedButtonStyle));
}