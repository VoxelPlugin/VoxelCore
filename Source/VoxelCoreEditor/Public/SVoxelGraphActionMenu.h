// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "SGraphActionMenu.h"

class VOXELCOREEDITOR_API SVoxelGraphActionMenu : public SGraphActionMenu
{
public:
	void Construct(
		const FArguments& InArgs,
		bool bReadOnly,
		const TSharedPtr<SScrollBar>& ExternalScrollBar);

	TSharedPtr<FGraphActionNode> FindItemByName(
		FName ItemName,
		int32 SectionId) const;

	FSlateRect GetPaintSpaceEntryBounds(
		const TSharedPtr<FGraphActionNode>& Node,
		bool bIncludeChildren) const;

	FSlateRect GetTickSpaceEntryBounds(
		const TSharedPtr<FGraphActionNode>& Node,
		bool bIncludeChildren) const;
};