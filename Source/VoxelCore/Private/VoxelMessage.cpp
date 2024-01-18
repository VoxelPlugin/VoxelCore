// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMessage.h"
#include "VoxelMessageTokens.h"
#include "Logging/TokenizedMessage.h"

TSharedRef<IMessageToken> FVoxelMessageToken::GetMessageToken() const
{
	return FTextToken::Create(FText::FromString(ToString()));
}

void FVoxelMessageToken_Group::AddText(const FString& Text)
{
	AddToken(FVoxelMessageTokenFactory::CreateTextToken(Text));
}

void FVoxelMessageToken_Group::AddToken(const TSharedRef<FVoxelMessageToken>& Token)
{
	Tokens.Add(Token);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelMessage> FVoxelMessage::Create(const EVoxelMessageSeverity Severity)
{
	return MakeVoxelShareable(new (GVoxelMemory) FVoxelMessage(Severity));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMessage::AddText(const FString& Text)
{
	AddToken(FVoxelMessageTokenFactory::CreateTextToken(Text));
}

void FVoxelMessage::AddToken(const TSharedRef<FVoxelMessageToken>& Token)
{
	if (const FVoxelMessageToken_Group* GroupToken = Cast<FVoxelMessageToken_Group>(*Token))
	{
		for (const TSharedRef<FVoxelMessageToken>& OtherToken : GroupToken->GetTokens())
		{
			AddToken(OtherToken);
		}
		return;
	}

	if (Tokens.Num() > 0 &&
		Tokens.Last()->TryMerge(*Token))
	{
		return;
	}

	Tokens.Add(Token);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint64 FVoxelMessage::GetHash() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<uint32> Data;
	Data.Add(uint32(Severity));
	Data.Add(Tokens.Num());

	for (const TSharedRef<FVoxelMessageToken>& Token : Tokens)
	{
		Data.Add(Token->GetHash());
	}

	return FVoxelUtilities::MurmurHashBytes(MakeByteVoxelArrayView(Data));
}

FString FVoxelMessage::ToString() const
{
	FString Result;
	for (const TSharedRef<FVoxelMessageToken>& Token : Tokens)
	{
		Result += Token->ToString();
	}
	return Result;
}

TSet<const UObject*> FVoxelMessage::GetObjects() const
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	TSet<const UObject*> Result;
	for (const TSharedRef<FVoxelMessageToken>& Token : Tokens)
	{
		Token->GetObjects(Result);
	}
	Result.Remove(nullptr);
	return Result;
}

EMessageSeverity::Type FVoxelMessage::GetMessageSeverity() const
{
	switch (Severity)
	{
	default: VOXEL_ASSUME(false);
	case EVoxelMessageSeverity::Info: return EMessageSeverity::Info;
	case EVoxelMessageSeverity::Warning: return EMessageSeverity::Warning;
	case EVoxelMessageSeverity::Error: return EMessageSeverity::Error;
	}
}

TSharedRef<FTokenizedMessage> FVoxelMessage::CreateTokenizedMessage() const
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FTokenizedMessage> TokenizedMessage = FTokenizedMessage::Create(GetMessageSeverity());

	for (const TSharedRef<FVoxelMessageToken>& Token : Tokens)
	{
		TokenizedMessage->AddToken(Token->GetMessageToken());
	}

	return TokenizedMessage;
}