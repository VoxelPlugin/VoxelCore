// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class VOXELCORE_API FVoxelNotification : public TSharedFromThis<FVoxelNotification>
{
public:
	static TSharedRef<FVoxelNotification> Create(const FString& Text);
	static TSharedRef<FVoxelNotification> Create_Failed(const FString& Text);

public:
	void ExpireNow();
	void ExpireAndFadeout();
	void ExpireAndFadeoutIn(float Delay);
	void ResetExpiration();

	void SetText(const FString& Text);
	void SetSubText(const FString& Text);

	void MarkAsPending();
	void MarkAsFailedAndExpire(float Delay);
	void MarkAsCompletedAndExpire(float Delay);

	void AddButton(
		const FString& Text,
		const FString& ToolTip,
		const TFunction<void()>& OnClicked);

	void AddButton_ExpireOnClick(
		const FString& Text,
		const FString& ToolTip,
		const TFunction<void()>& OnClicked);

private:
	const TWeakPtr<SNotificationItem> WeakNotification;
	FString PrivateText;
	FString PrivateSubText;

	explicit FVoxelNotification(const TSharedPtr<SNotificationItem>& Notification)
		: WeakNotification(Notification)
	{
	}

	static TSharedRef<FVoxelNotification> CreateImpl(
		const TSharedPtr<SNotificationItem>& Notification,
		const FString& Text);
};