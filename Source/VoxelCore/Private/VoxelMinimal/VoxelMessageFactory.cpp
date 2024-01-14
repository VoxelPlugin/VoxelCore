// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMessageTokens.h"

TSharedRef<FVoxelMessageToken> FVoxelMessageTokenFactory::CreateTextToken(const FString& Text)
{
	ensure(!Text.Contains(TEXT("%d")));
	ensure(!Text.Contains(TEXT("%l")));
	ensure(!Text.Contains(TEXT("%f")));
	ensure(!Text.Contains(TEXT("%s")));

	const TSharedRef<FVoxelMessageToken_Text> Result = MakeVoxelShared<FVoxelMessageToken_Text>();
	Result->Text = Text;
	return Result;
}

TSharedRef<FVoxelMessageToken> FVoxelMessageTokenFactory::CreatePinToken(const UEdGraphPin* Pin)
{
	ensure(IsInGameThread());

	const TSharedRef<FVoxelMessageToken_Pin> Result = MakeVoxelShared<FVoxelMessageToken_Pin>();
	Result->PinReference = Pin;
	return Result;
}

TSharedRef<FVoxelMessageToken> FVoxelMessageTokenFactory::CreateObjectToken(const TWeakObjectPtr<const UObject> WeakObject)
{
	const TSharedRef<FVoxelMessageToken_Object> Result = MakeVoxelShared<FVoxelMessageToken_Object>();
	Result->WeakObject = WeakObject;
	return Result;
}

TSharedRef<FVoxelMessageToken> FVoxelMessageTokenFactory::CreateArrayToken(const TVoxelArray<TSharedRef<FVoxelMessageToken>>& Tokens)
{
	if (Tokens.Num() == 0)
	{
		return CreateTextToken("Empty");
	}

	const TSharedRef<FVoxelMessageToken_Group> Result = MakeVoxelShared<FVoxelMessageToken_Group>();
	Result->AddToken(Tokens[0]);

	for (int32 Index = 1; Index < Tokens.Num(); Index++)
	{
		Result->AddText(", ");
		Result->AddToken(Tokens[Index]);
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DEFINE(Type) \
	TSharedRef<FVoxelMessageToken> TVoxelMessageTokenFactory<Type>::CreateToken(std::add_const_t<Type>& Value)

DEFINE(FText)
{
	return CreateTextToken(Value.ToString());
}
DEFINE(const char*)
{
	return CreateTextToken(FString(Value));
}
DEFINE(const TCHAR*)
{
	return CreateTextToken(Value);
}
DEFINE(FName)
{
	return CreateTextToken(Value.ToString());
}
DEFINE(FString)
{
	return CreateTextToken(Value);
}
DEFINE(FScriptInterface)
{
	ensure(IsInGameThread());
	return CreateObjectToken(Value.GetObject());
}

DEFINE(int8)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(int16)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(int32)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(int64)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}

DEFINE(uint8)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(uint16)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(uint32)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(uint64)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}

DEFINE(float)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}
DEFINE(double)
{
	return CreateTextToken(FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping()).ToString());
}

#undef DEFINE