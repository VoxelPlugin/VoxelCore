// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Widgets/Notifications/INotificationWidget.h"

class VOXELCOREEDITOR_API SVoxelNotification
	: public SCompoundWidget
	, public INotificationWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(int32, Count);
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredWidth)
	};

	void Construct(
		const FArguments& Args,
		const TSharedRef<FVoxelMessage>& Message);

	//~ Begin INotificationWidget Interface
	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override
	{
	}
	virtual TSharedRef<SWidget> AsWidget() override
	{
		return AsShared();
	}
	//~ End INotificationWidget Interface

private:
	static TSharedRef<SWidget> CreateHyperlink(
		const TSharedRef<IMessageToken>& MessageToken,
		const FText& Tooltip);

	static void CreateMessage(
		const TSharedRef<SVerticalBox>& VBox,
		TSharedPtr<SHorizontalBox>& HBox,
		const TSharedRef<IMessageToken>& MessageToken,
		float Padding);
};