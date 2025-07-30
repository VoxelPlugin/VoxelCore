// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#if WITH_EDITOR
#include "Animation/CurveHandle.h"
#include "Animation/CurveSequence.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Components/HorizontalBox.h"
#include "Application/ThrottleManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#endif

TSharedRef<FVoxelNotification> FVoxelNotification::Create(const FString& Text)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	FNotificationInfo Info(FText{});
	Info.bFireAndForget = false;
	Info.bUseThrobber = true;

	TSharedPtr<SNotificationItem> Notification;

	// FSlateNotificationManager is only valid if slate is initialized
	// Will fail in commandlets
	if (FSlateApplication::IsInitialized())
	{
		Notification = FSlateNotificationManager::Get().AddNotification(Info);
	}

	if (Notification)
	{
		Notification->SetCompletionState(SNotificationItem::CS_None);
	}

	return CreateImpl(Notification, Text);
#else
	return MakeShareable(new FVoxelNotification(nullptr));
#endif
}

TSharedRef<FVoxelNotification> FVoxelNotification::Create_Failed(const FString& Text)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	FNotificationInfo Info(FText{});
	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = true;

	TSharedPtr<SNotificationItem> Notification;

	// FSlateNotificationManager is only valid if slate is initialized
	// Will fail in commandlets
	if (FSlateApplication::IsInitialized())
	{
		Notification = FSlateNotificationManager::Get().AddNotification(Info);
	}

	if (Notification)
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}

	return CreateImpl(Notification, Text);
#else
	return MakeShareable(new FVoxelNotification(nullptr));
#endif
}

void FVoxelNotification::ExpireNow()
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	ResetExpiration();

	Notification->SetExpireDuration(0.f);
	Notification->SetFadeOutDuration(0.f);
	Notification->ExpireAndFadeout();
#endif
}

void FVoxelNotification::ExpireAndFadeout()
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	ResetExpiration();

	Notification->SetExpireDuration(0.f);
	Notification->ExpireAndFadeout();
#endif
}

void FVoxelNotification::ExpireAndFadeoutIn(const float Delay)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	ResetExpiration();

	Notification->SetExpireDuration(Delay);
	Notification->ExpireAndFadeout();
#endif
}

void FVoxelNotification::ResetExpiration()
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	struct SNotificationExtendable : public SNotificationItem
	{
		TWeakPtr<SNotificationList> MyList;
		TAttribute<FText> Text;
		TAttribute<FText> SubText;
		TAttribute<float> FadeInDuration;
		TAttribute<float> FadeOutDuration;
		TAttribute<float> ExpireDuration;
		TAttribute<const FSlateBrush*> BaseIcon;
		FLinearColor DefaultGlowColor = FLinearColor(1.0f, 1.0f, 1.0f);
		TSharedPtr<STextBlock> MyTextBlock;
		TSharedPtr<STextBlock> MySubTextBlock;
		ECompletionState CompletionState;
		FCurveSequence FadeAnimation;
		FCurveHandle FadeCurve;
		FCurveSequence IntroAnimation;
		FCurveHandle ScaleCurveX;
		FCurveHandle ScaleCurveY;
		FCurveHandle GlowCurve;
		FCurveSequence ThrobberAnimation;
		FCurveSequence CompletionStateAnimation;
		FThrottleRequest ThrottleHandle;
		FTSTicker::FDelegateHandle TickDelegateHandle;

		float InternalTime = 0.0f;
		bool bAutoExpire = false;
	};

	float& InternalTime = static_cast<SNotificationExtendable&>(*Notification).InternalTime;
	if (!ensure(0.f <= InternalTime) ||
		!ensure(InternalTime < 1.e6))
	{
		// SNotificationExtendable layout changed?
		return;
	}

	InternalTime = 0;
#endif
}

void FVoxelNotification::SetText(const FString& Text)
{
	PrivateText = Text;
}

void FVoxelNotification::SetSubText(const FString& Text)
{
	PrivateSubText = Text;
}

void FVoxelNotification::MarkAsPending()
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	Notification->SetCompletionState(SNotificationItem::CS_Pending);
#endif
}

void FVoxelNotification::MarkAsFailedAndExpire(const float Delay)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	Notification->SetCompletionState(SNotificationItem::CS_Fail);

	ExpireAndFadeoutIn(Delay);
#endif
}

void FVoxelNotification::MarkAsCompletedAndExpire(const float Delay)
{
	VOXEL_FUNCTION_COUNTER();

#if WITH_EDITOR
	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	Notification->SetCompletionState(SNotificationItem::CS_Success);

	ExpireAndFadeoutIn(Delay);
#endif
}

void FVoxelNotification::AddButton(
	const FString& Text,
	const FString& ToolTip,
	const TFunction<void()>& OnClicked)
{
#if WITH_EDITOR
	VOXEL_FUNCTION_COUNTER();
	check(OnClicked);

	const TSharedPtr<SNotificationItem> Notification = WeakNotification.Pin();
	if (!Notification)
	{
		return;
	}

	// See SNotificationItemImpl

	if (!ensure(Notification->GetChildren()->Num() == 1))
	{
		return;
	}
	const TSharedRef<SWidget> NotificationBackground = Notification->GetChildren()->GetChildAt(0);

	if (!ensure(NotificationBackground->GetChildren()->Num() == 1))
	{
		return;
	}
	const TSharedRef<SWidget> Box = NotificationBackground->GetChildren()->GetChildAt(0);

	if (!ensure(Box->GetChildren()->Num() == 1))
	{
		return;
	}
	const TSharedRef<SWidget> HorizontalBox = Box->GetChildren()->GetChildAt(0);

	if (!ensure(HorizontalBox->GetChildren()->Num() == 2))
	{
		return;
	}
	const TSharedRef<SWidget> InteractiveWidgetsBox = HorizontalBox->GetChildren()->GetChildAt(1);

	if (!ensure(InteractiveWidgetsBox->GetChildren()->Num() == 5))
	{
		return;
	}
	const TSharedRef<SWidget> ButtonsBox = InteractiveWidgetsBox->GetChildren()->GetChildAt(4);

	if (!ensure(ButtonsBox->GetType() == "SHorizontalBox"))
	{
		return;
	}

	static_cast<SHorizontalBox&>(*ButtonsBox).AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SButton)
			.Text(FText::FromString(Text))
			.ToolTipText(FText::FromString(ToolTip))
			.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NotificationList.WidgetText"))
			.OnClicked_Lambda([OnClicked]
			{
				OnClicked();
				return FReply::Handled();
			})
		];
#endif
}

void FVoxelNotification::AddButton_ExpireOnClick(
	const FString& Text,
	const FString& ToolTip,
	const TFunction<void()>& OnClicked)
{
	AddButton(
		Text,
		ToolTip,
		[OnClicked = OnClicked, WeakThis = AsWeak()]
		{
			OnClicked();

			if (const TSharedPtr<FVoxelNotification> This = WeakThis.Pin())
			{
				This->ExpireNow();
			}
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelNotification> FVoxelNotification::CreateImpl(
	const TSharedPtr<SNotificationItem>& Notification,
	const FString& Text)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelNotification> Result = MakeShareable(new FVoxelNotification(Notification));

#if WITH_EDITOR
	if (Notification)
	{
		// Keep Result alive so it has the same lifetime as Notification
		Notification->SetText(MakeAttributeLambda([Result]
		{
			return FText::FromString(Result->PrivateText);
		}));
		Notification->SetSubText(MakeAttributeLambda([Result]
		{
			return FText::FromString(Result->PrivateSubText);
		}));
	}
#endif

	Result->SetText(Text);
	return Result;
}