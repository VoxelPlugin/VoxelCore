// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelMessage.h"
#include "SVoxelNotification.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "Logging/MessageLog.h"

class FVoxelMessagesEditor : public FVoxelEditorSingleton
{
public:
	//~ Begin FVoxelEditorSingleton Interface
	virtual void Initialize() override
	{
		GVoxelMessageManager->OnMessageLogged.AddLambda([this](const TSharedRef<FVoxelMessage>& Message)
		{
			LogMessage(Message);
		});

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowFilters = true;
		InitOptions.bShowPages = false;
		InitOptions.bAllowClear = true;
		MessageLogModule.RegisterLogListing("Voxel", INVTEXT("Voxel"), InitOptions);

		Voxel::OnRefreshAll.AddLambda([]
		{
			FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog").GetLogListing("Voxel")->ClearMessages();
		});
	}
	//~ End FVoxelEditorSingleton Interface

public:
	void LogMessage(const TSharedRef<FVoxelMessage>& Message)
	{
		const uint64 Hash = Message->GetHash();
		const double Time = FPlatformTime::Seconds();

		bool bFoundRecentMessage = false;
		for (int32 Index = 0; Index < RecentMessages.Num(); Index++)
		{
			FRecentMessage& RecentMessage = RecentMessages[Index];
			if (RecentMessage.LastTime + 5 < Time)
			{
				RecentMessages.RemoveAtSwap(Index);
				Index--;
				continue;
			}

			if (RecentMessage.Hash == Hash)
			{
				bFoundRecentMessage = true;
				RecentMessage.LastTime = Time;
				continue;
			}
		}

		if (!bFoundRecentMessage)
		{
			RecentMessages.Add(FRecentMessage
			{
				Hash,
				Time
			});
		}

		if (RecentMessages.Num() > 3)
		{
			for (const TSharedPtr<FNotification>& Notification : Notifications)
			{
				if (const TSharedPtr<SNotificationItem> Item = Notification->WeakItem.Pin())
				{
					Item->SetExpireDuration(0);
					Item->SetFadeOutDuration(0);
					Item->ExpireAndFadeout();
				}
			}
			Notifications.Reset();

			const FText ErrorText = FText::FromString(FString::FromInt(RecentMessages.Num()) + " voxel errors");

			if (const TSharedPtr<SNotificationItem> Item = WeakGlobalNotification.Pin())
			{
				Item->SetText(ErrorText);
				// Reset expiration
				Item->ExpireAndFadeout();
				return;
			}

			FNotificationInfo Info = FNotificationInfo(ErrorText);
			Info.CheckBoxState = ECheckBoxState::Unchecked;
			Info.ExpireDuration = 10;
			Info.WidthOverride = FOptionalSize();

			Info.ButtonDetails.Add(FNotificationButtonInfo(
				INVTEXT("Dismiss"),
				FText(),
				MakeLambdaDelegate([this]
				{
					if (const TSharedPtr<SNotificationItem> Item = WeakGlobalNotification.Pin())
					{
						Item->SetExpireDuration(0);
						Item->SetFadeOutDuration(0);
						Item->ExpireAndFadeout();
					}
				}),
				SNotificationItem::CS_Fail));

			Info.ButtonDetails.Add(FNotificationButtonInfo(
				INVTEXT("Show Message Log"),
				FText(),
				MakeLambdaDelegate([this]
				{
					FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
					MessageLogModule.OpenMessageLog("Voxel");
				}),
				SNotificationItem::CS_Fail));

			const TSharedPtr<SNotificationItem> GlobalNotification = FSlateNotificationManager::Get().AddNotification(Info);
			if (!ensure(GlobalNotification))
			{
				return;
			}

			GlobalNotification->SetCompletionState(SNotificationItem::CS_Fail);
			WeakGlobalNotification = GlobalNotification;

			return;
		}

		for (int32 Index = 0; Index < Notifications.Num(); Index++)
		{
			const TSharedPtr<FNotification> Notification = Notifications[Index];
			const TSharedPtr<SNotificationItem> Item = Notification->WeakItem.Pin();
			if (!Item.IsValid())
			{
				Notifications.RemoveAtSwap(Index);
				Index--;
				continue;
			}

			if (Notification->Hash != Hash)
			{
				continue;
			}

			Notification->Count++;
			Item->ExpireAndFadeout();
			return;
		}

		const TSharedRef<FNotification> Notification = MakeShared<FNotification>();
		Notification->Text = FText::FromString(Message->ToString());
		Notification->Hash = Hash;

		FNotificationInfo Info = FNotificationInfo(Notification->Text);
		Info.CheckBoxState = ECheckBoxState::Unchecked;
		Info.ExpireDuration = 10;
		Info.WidthOverride = FOptionalSize();
		Info.ContentWidget =
			SNew(SVoxelNotification, Message)
			.Count_Lambda([=]
			{
				return Notification->Count;
			})
			.MaxDesiredWidth(1000.f);

		Notification->WeakItem = FSlateNotificationManager::Get().AddNotification(Info);
		Notifications.Add(Notification);
	}

private:
	TWeakPtr<SNotificationItem> WeakGlobalNotification;

	struct FRecentMessage
	{
		uint64 Hash = 0;
		double LastTime = 0;
	};
	TVoxelArray<FRecentMessage> RecentMessages;

	struct FNotification
	{
		TWeakPtr<SNotificationItem> WeakItem;
		FText Text;
		uint64 Hash = 0;
		int32 Count = 1;
	};
	TVoxelArray<TSharedPtr<FNotification>> Notifications;
};
FVoxelMessagesEditor* GVoxelMessagesEditor = new FVoxelMessagesEditor();