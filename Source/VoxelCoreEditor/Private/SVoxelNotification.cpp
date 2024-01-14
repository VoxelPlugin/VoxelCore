// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelNotification.h"
#include "VoxelMessage.h"
#include "Misc/UObjectToken.h"
#include "Internationalization/Regex.h"

void SVoxelNotification::Construct(
	const FArguments& Args,
	const TSharedRef<FVoxelMessage>& Message)
{
	SetToolTipText(FText::FromString(Message->ToString()));

	const TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);

	TSharedPtr<SHorizontalBox> HBox;
	VBox->AddSlot()
	.AutoHeight()
	[
		SAssignNew(HBox, SHorizontalBox)
	];

	const TSharedRef<FTokenizedMessage> TokenizedMessage = Message->CreateTokenizedMessage();
	for (const TSharedRef<IMessageToken>& Token : TokenizedMessage->GetMessageTokens())
	{
		CreateMessage(VBox, HBox, Token, 2.0f);
	}

	HBox->AddSlot();

	HBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility_Lambda([=]
			{
				return Args._Count.Get() <= 1 ? EVisibility::Collapsed : EVisibility::Visible;
			})
			.TextStyle(FAppStyle::Get(), "MessageLog")
			.Text_Lambda([=]
			{
				return FText::Format(INVTEXT(" (x{0})"), FText::AsNumber(Args._Count.Get()));
			})
		];

	HBox->AddSlot()
		.AutoWidth()
		[
			SNullWidget::NullWidget
		];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Padding(2.0f)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush(FTokenizedMessage::GetSeverityIconName(TokenizedMessage->GetSeverity())))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(Args._MaxDesiredWidth)
			[
				VBox
			]
		]
	];
}

TSharedRef<SWidget> SVoxelNotification::CreateHyperlink(const TSharedRef<IMessageToken>& MessageToken, const FText& Tooltip)
{
	return
		SNew(SHyperlink)
		.Text(MessageToken->ToText())
		.ToolTipText(Tooltip)
		.TextStyle(FAppStyle::Get(), "MessageLog")
		.OnNavigate_Lambda([=]
		{
		   MessageToken->GetOnMessageTokenActivated().ExecuteIfBound(MessageToken);
		});
}

void SVoxelNotification::CreateMessage(const TSharedRef<SVerticalBox>& VBox, TSharedPtr<SHorizontalBox>& HBox, const TSharedRef<IMessageToken>& MessageToken, const float Padding)
{
	const auto AddWidget = [&](
		const TSharedRef<SWidget>& Widget,
		const FName IconName = {},
		const TAttribute<EVisibility>& WidgetVisibility = {})
	{
		const TSharedRef<SHorizontalBox> ChildHBox = SNew(SHorizontalBox);

		if (WidgetVisibility.IsBound())
		{
			ChildHBox->SetVisibility(WidgetVisibility);
		}

		if (!IconName.IsNone())
		{
			ChildHBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush(IconName))
			];
		}

		ChildHBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f, 0.0f, 0.0f)
		[
			Widget
		];

		HBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(Padding, 0.0f, 0.0f, 0.0f)
		[
			ChildHBox
		];
	};

	switch (MessageToken->GetType())
	{
	default:
	{
		ensure(false);
		return;
	}

	case EMessageToken::Severity:
	{
		return;
	}

	case EMessageToken::Image:
	{
		const TSharedRef<FImageToken> ImageToken = StaticCastSharedRef<FImageToken>(MessageToken);

		if (ImageToken->GetImageName().IsNone())
		{
			return;
		}

		if (MessageToken->GetOnMessageTokenActivated().IsBound())
		{
			AddWidget(
				SNew(SButton)
				.OnClicked_Lambda([=]
				{
					MessageToken->GetOnMessageTokenActivated().ExecuteIfBound(MessageToken);
					return FReply::Handled();
				})
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush(ImageToken->GetImageName()))
				]);
		}
		else
		{
			AddWidget(
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(ImageToken->GetImageName()))
			);
		}
	}
	break;

	case EMessageToken::Object:
	{
		const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(MessageToken);

		UObject* Object;

		// Due to blueprint reconstruction, we can't directly use the Object as it will get trashed during the blueprint reconstruction and the message token will no longer point to the right UObject.
		// Instead we will retrieve the object from the name which should always be good.
		if (UObjectToken->GetObject().IsValid())
		{
			if (!UObjectToken->ToText().ToString().Equals(UObjectToken->GetObject().Get()->GetName()))
			{
				Object = FindObject<UObject>(nullptr, *UObjectToken->GetOriginalObjectPathName());
			}
			else
			{
				Object = UObjectToken->GetObject().Get();
			}
		}
		else
		{
			// We have no object (probably because is now stale), try finding the original object linked to this message token to see if it still exist
			Object = FindObject<UObject>(nullptr, *UObjectToken->GetOriginalObjectPathName());
		}

		AddWidget(
			CreateHyperlink(MessageToken, FUObjectToken::DefaultOnGetObjectDisplayName().IsBound() ? FUObjectToken::DefaultOnGetObjectDisplayName().Execute(Object, true) : UObjectToken->ToText()),
			"Icons.Search");
	}
	break;

	case EMessageToken::URL:
	{
		const TSharedRef<FURLToken> URLToken = StaticCastSharedRef<FURLToken>(MessageToken);

		AddWidget(
			CreateHyperlink(MessageToken, FText::FromString(URLToken->GetURL())),
			"MessageLog.Url");
	}
	break;

	case EMessageToken::EdGraph:
	{
		AddWidget(
			CreateHyperlink(MessageToken, MessageToken->ToText()),
			"Icons.Search");
	}
	break;

	case EMessageToken::Action:
	{
		const TSharedRef<FActionToken> ActionToken = StaticCastSharedRef<FActionToken>(MessageToken);

		const TSharedRef<SHyperlink> Widget =
			SNew(SHyperlink)
			.Text(MessageToken->ToText())
			.ToolTipText(ActionToken->GetActionDescription())
			.TextStyle(FAppStyle::Get(), "MessageLog")
			.OnNavigate_Lambda([=]
			{
				ActionToken->ExecuteAction();
			});

		AddWidget(
			Widget,
			"MessageLog.Action",
			MakeAttributeLambda([=]
			{
				return ActionToken->CanExecuteAction() ? EVisibility::Visible : EVisibility::Collapsed;
			}));
	}
	break;

	case EMessageToken::AssetName:
	{
		const TSharedRef<FAssetNameToken> AssetNameToken = StaticCastSharedRef<FAssetNameToken>(MessageToken);

		AddWidget(
			CreateHyperlink(MessageToken, AssetNameToken->ToText()),
			"Icons.Search");
	}
	break;

	case EMessageToken::DynamicText:
	{
		const TSharedRef<FDynamicTextToken> TextToken = StaticCastSharedRef<FDynamicTextToken>(MessageToken);

		AddWidget(
			SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(TextToken->GetTextAttribute()));
	}
	break;

	case EMessageToken::Text:
	{
		if (MessageToken->GetOnMessageTokenActivated().IsBound())
		{
			AddWidget(CreateHyperlink(MessageToken, MessageToken->ToText()));
			return;
		}

		FString MessageString = MessageToken->ToText().ToString();
		if (MessageString == "\n")
		{
			VBox->AddSlot()
			.AutoHeight()
			[
				SAssignNew(HBox, SHorizontalBox)
			];
			return;
		}

		// ^((?:[\w]\:|\\)(?:(?:\\[a-z_\-\s0-9\.]+)+)\.(?:cpp|h))\((\d+)\)
		// https://regex101.com/r/vV4cV7/1
		const FRegexPattern FileAndLinePattern(TEXT("^((?:[\\w]\\:|\\\\)(?:(?:\\\\[a-z_\\-\\s0-9\\.]+)+)\\.(?:cpp|h))\\((\\d+)\\)"));
		FRegexMatcher FileAndLineRegexMatcher(FileAndLinePattern, MessageString);

		TSharedRef<SWidget> SourceLink = SNullWidget::NullWidget;

		if (FileAndLineRegexMatcher.FindNext())
		{
			const FString FileName = FileAndLineRegexMatcher.GetCaptureGroup(1);
			const int32 LineNumber = FCString::Atoi(*FileAndLineRegexMatcher.GetCaptureGroup(2));

			// Remove the hyperlink from the message, since we're splitting it into its own string.
			MessageString.RightChopInline(FileAndLineRegexMatcher.GetMatchEnding(), false);

			SourceLink =
				SNew(SHyperlink)
				.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
				.TextStyle(FAppStyle::Get(), "MessageLog")
				.OnNavigate_Lambda([=] { FSlateApplication::Get().GotoLineInSource(FileName, LineNumber); })
				.Text(FText::FromString(FileAndLineRegexMatcher.GetCaptureGroup(0)));
		}

		const TSharedRef<SHorizontalBox> Widget =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0)
			[
				SourceLink
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 4.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(MessageString))
				.ColorAndOpacity(FSlateColor::UseForeground())
				.TextStyle(FAppStyle::Get(), "MessageLog")
			];

		AddWidget(Widget);
	}
	break;

	case EMessageToken::Actor:
	{
		const TSharedRef<FActorToken> ActorToken = StaticCastSharedRef<FActorToken>(MessageToken);

		AddWidget(
			CreateHyperlink(MessageToken, ActorToken->ToText()),
			"Icons.Search");
	}
	break;
	}
}