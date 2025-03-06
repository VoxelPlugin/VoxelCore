// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMessage.h"
#include "Logging/MessageLog.h"
#include "Framework/Application/SlateApplication.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

FVoxelMessageManager* GVoxelMessageManager = new FVoxelMessageManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelMessagesThreadSingleton : public TThreadSingleton<FVoxelMessagesThreadSingleton>
{
public:
	// Weak ptr because messages are built on the game thread
	TArray<TWeakPtr<IVoxelMessageConsumer>> MessageConsumers;

	TWeakPtr<IVoxelMessageConsumer> GetTop() const
	{
		if (MessageConsumers.Num() == 0)
		{
			return nullptr;
		}
		return MessageConsumers.Last();
	}
};

FVoxelScopedMessageConsumer::FVoxelScopedMessageConsumer(TWeakPtr<IVoxelMessageConsumer> MessageConsumer)
{
	FVoxelMessagesThreadSingleton::Get().MessageConsumers.Add(MessageConsumer);
}

FVoxelScopedMessageConsumer::FVoxelScopedMessageConsumer(TFunction<void(const TSharedRef<FVoxelMessage>&)> LogMessage)
{
	class FMessageConsumer : public IVoxelMessageConsumer
	{
	public:
		TFunction<void(const TSharedRef<FVoxelMessage>&)> LogMessageLambda;

		virtual void LogMessage(const TSharedRef<FVoxelMessage>& Message) override
		{
			LogMessageLambda(Message);
		}
	};

	const TSharedRef<FMessageConsumer> Consumer = MakeShared<FMessageConsumer>();
	Consumer->LogMessageLambda = LogMessage;

	TempConsumer = Consumer;
	FVoxelMessagesThreadSingleton::Get().MessageConsumers.Add(Consumer);
}

FVoxelScopedMessageConsumer::~FVoxelScopedMessageConsumer()
{
	FVoxelMessagesThreadSingleton::Get().MessageConsumers.Pop(EAllowShrinking::No);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMessageManager::LogMessage(const TSharedRef<FVoxelMessage>& Message)
{
	VOXEL_FUNCTION_COUNTER();

	for (const FGatherCallstack& GatherCallstack : GatherCallstacks)
	{
		GatherCallstack(Message);
	}

	const TSharedPtr<IVoxelMessageConsumer> MessageConsumer = FVoxelMessagesThreadSingleton::Get().GetTop().Pin();

	// Only check recent messages if we don't have a message consumer, otherwise graph errors will get silenced
	if (!MessageConsumer)
	{
		VOXEL_SCOPE_COUNTER("Check recent messages");
		VOXEL_SCOPE_LOCK(CriticalSection);

		const uint64 Hash = Message->GetHash();
		const double Time = FPlatformTime::Seconds();
		const uint64 FrameCounter = GFrameCounter;

		if (const FMessageTime* MessageTime = HashToMessageTime_RequiresLock.Find(Hash))
		{
			// Also check FrameCounter in case the game thread is lagging, to avoid always adding the same message
			if (Time < MessageTime->Time + 0.5 ||
				FrameCounter < MessageTime->FrameCounter + 10)
			{
				return;
			}
		}

		HashToMessageTime_RequiresLock.FindOrAdd(Hash) = FMessageTime
		{
			Time,
			FrameCounter
		};
	}

	Voxel::GameTask([=]
	{
		VOXEL_SCOPE_COUNTER("Log");

		if (MessageConsumer)
		{
			MessageConsumer->LogMessage(Message);
		}
		else
		{
			GVoxelMessageManager->LogMessage_GameThread(Message);
		}
	});
}

void FVoxelMessageManager::LogMessage_GameThread(const TSharedRef<FVoxelMessage>& Message) const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (NO_LOGGING)
	{
		return;
	}

	if (!GIsEditor)
	{
		switch (Message->GetSeverity())
		{
		default: VOXEL_ASSUME(false);
		case EVoxelMessageSeverity::Info:
		{
			LOG_VOXEL(Log, "%s", *Message->ToString());
		}
		break;
		case EVoxelMessageSeverity::Warning:
		{
			LOG_VOXEL(Warning, "%s", *Message->ToString());
		}
		break;
		case EVoxelMessageSeverity::Error:
		{
			if (IsRunningCookCommandlet() ||
				IsRunningCookOnTheFly())
			{
				// Don't fail cooking
				LOG_VOXEL(Warning, "%s", *Message->ToString());
			}
			else
			{
				LOG_VOXEL(Error, "%s", *Message->ToString());
			}
		}
		break;
		}

		OnMessageLogged.Broadcast(Message);
		return;
	}

#if WITH_EDITOR
	const auto LogMessage = [=]
	{
		GVoxelMessageManager->OnMessageLogged.Broadcast(Message);

		const TSharedRef<FTokenizedMessage> TokenizedMessage = Message->CreateTokenizedMessage();

		if (FVoxelUtilities::IsPlayInEditor())
		{
			FMessageLog("PIE").AddMessage(TokenizedMessage);
		}

		FMessageLog("Voxel").AddMessage(TokenizedMessage);
	};

	if (FSlateApplication::IsInitialized() &&
		FSlateApplication::Get().GetActiveModalWindow())
	{
		// DelayedCall would only run once the modal is closed
		LogMessage();
		return;
	}

	FVoxelUtilities::DelayedCall(LogMessage);
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMessageManager::LogMessageFormat(const EVoxelMessageSeverity Severity, const TCHAR* Format)
{
	if (NO_LOGGING)
	{
		return;
	}

	GVoxelMessageManager->InternalLogMessageFormat(Severity, Format, {});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMessageManager::InternalLogMessageFormat(
	const EVoxelMessageSeverity Severity,
	const TCHAR* Format,
	const TConstVoxelArrayView<TSharedRef<FVoxelMessageToken>> Tokens)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<bool> UsedTokens;
	UsedTokens.SetNumZeroed(Tokens.Num());

	const TSharedRef<FVoxelMessage> Message = FVoxelMessage::Create(Severity);

	while (Format)
	{
		// Read to next "{":
		const TCHAR* Delimiter = FCString::Strstr(Format, TEXT("{"));
		if (!Delimiter)
		{
			// Add the remaining text
			const FString RemainingText(Format);
			if (!RemainingText.IsEmpty())
			{
				Message->AddText(RemainingText);
			}
			break;
		}

		// Add the text left of the {
		const FString TextBefore(Delimiter - Format, Format);
		if (!TextBefore.IsEmpty())
		{
			Message->AddText(TextBefore);
		}

		Format = Delimiter + FCString::Strlen(TEXT("{"));

		// Read to next "}":
		Delimiter = FCString::Strstr(Format, TEXT("}"));
		if (!ensureMsgf(Delimiter, TEXT("Missing }")))
		{
			break;
		}

		const FString IndexString(Delimiter - Format, Format);
		if (!ensure(FCString::IsNumeric(*IndexString)))
		{
			break;
		}

		const int32 Index = FCString::Atoi(*IndexString);
		if (!ensureMsgf(Tokens.IsValidIndex(Index), TEXT("Out of bound index: {%d}"), Index))
		{
			break;
		}

		UsedTokens[Index] = true;
		Message->AddToken(Tokens[Index]);

		Format = Delimiter + FCString::Strlen(TEXT("}"));
	}

	for (int32 Index = 0; Index < Tokens.Num(); Index++)
	{
		ensureMsgf(UsedTokens[Index], TEXT("Unused arg: %d"), Index);
	}

	LogMessage(Message);
}