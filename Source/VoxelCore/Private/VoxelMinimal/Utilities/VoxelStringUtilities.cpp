// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FText FVoxelUtilities::SecondsToText(double Value, const int32 NumExtraDigits)
{
	const TCHAR* Unit = TEXT("s");

	if (Value > 60)
	{
		Unit = TEXT("m");
		Value /= 60;

		if (Value > 60)
		{
			Unit = TEXT("h");
			Value /= 60;
		}
	}
	else if (Value < 1)
	{
		Unit = TEXT("ms");
		Value *= 1000;

		if (Value < 1)
		{
			Unit = TEXT("us");
			Value *= 1000;

			if (Value < 1)
			{
				Unit = TEXT("ns");
				Value *= 1000;
			}
		}
	}

	int32 NumDigits = 1;
	if (Value > 0)
	{
		while (Value * IntPow(10, NumDigits) < 1)
		{
			NumDigits++;
		}
	}

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = NumDigits + NumExtraDigits;
	Options.MinimumFractionalDigits = NumDigits;

	return FText::Format(INVTEXT("{0}{1}"), FText::AsNumber(Value, &Options), FText::FromString(Unit));
}

FText FVoxelUtilities::BytesToText(double Value)
{
	const TCHAR* Unit = TEXT("");

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 1;

	if (Value > 1024)
	{
		Options.MinimumFractionalDigits = 1;

		Unit = TEXT("K");
		Value /= 1024;

		if (Value > 1024)
		{
			Unit = TEXT("M");
			Value /= 1024;

			if (Value > 1024)
			{
				Unit = TEXT("G");
				Value /= 1024;

				if (Value > 1024)
				{
					Unit = TEXT("T");
					Value /= 1024;
				}
			}
		}
	}

	return FText::Format(INVTEXT("{0}{1}B"), FText::AsNumber(Value, &Options), FText::FromString(Unit));
}

FText FVoxelUtilities::NumberToText(double Value)
{
	const TCHAR* Unit = TEXT("");

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 1;

	if (Value > 1000)
	{
		Options.MinimumFractionalDigits = 1;

		Unit = TEXT("K");
		Value /= 1000;

		if (Value > 1000)
		{
			Unit = TEXT("M");
			Value /= 1000;

			if (Value > 1000)
			{
				Unit = TEXT("G");
				Value /= 1000;

				if (Value > 1000)
				{
					Unit = TEXT("T");
					Value /= 1000;
				}
			}
		}
	}

	return FText::Format(INVTEXT("{0}{1}"), FText::AsNumber(Value, &Options), FText::FromString(Unit));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelUtilities::SecondsToString(const double Value, const int32 NumExtraDigits)
{
	return SecondsToText(Value, NumExtraDigits).ToString();
}

FString FVoxelUtilities::BytesToString(const double Value)
{
	return BytesToText(Value).ToString();
}

FString FVoxelUtilities::NumberToString(const double Value)
{
	return NumberToText(Value).ToString();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelUtilities::IsInt(FStringView Text)
{
	if (Text.Len() == 0)
	{
		return false;
	}

	if (Text[0] == TEXT('-') ||
		Text[0] == TEXT('+'))
	{
		Text.RemovePrefix(1);
	}

	if (Text.Len() == 0)
	{
		return false;
	}

	for (const TCHAR Char : Text)
	{
		if (!FChar::IsDigit(Char))
		{
			return false;
		}
	}

	return true;
}

bool FVoxelUtilities::IsFloat(FStringView Text)
{
	if (Text.Len() == 0)
	{
		return false;
	}

	if (Text[0] == TEXT('-') ||
		Text[0] == TEXT('+'))
	{
		Text.RemovePrefix(1);
	}

	if (Text.Len() == 0)
	{
		return false;
	}

	if (Text[Text.Len() - 1] == TEXT('f'))
	{
		Text.RemoveSuffix(1);
	}

	if (Text.Len() == 0)
	{
		return false;
	}

	while (
		Text.Len() > 0 &&
		FChar::IsDigit(Text[0]))
	{
		Text.RemovePrefix(1);
	}

	if (Text.Len() == 0)
	{
		return true;
	}

	if (Text[0] == TEXT('.'))
	{
		Text.RemovePrefix(1);

		if (Text.Len() == 0)
		{
			return true;
		}

		while (
			Text.Len() > 0 &&
			FChar::IsDigit(Text[0]))
		{
			Text.RemovePrefix(1);
		}

		if (Text.Len() == 0)
		{
			return true;
		}
	}

	if (Text[0] == TEXT('e'))
	{
		Text.RemovePrefix(1);
		return IsInt(Text);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FName FVoxelUtilities::Concatenate(const FStringView& String, const FName Name)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	Builder.Append(String);
	AppendName(Builder, Name);
	return FName(Builder);
}

FName FVoxelUtilities::Concatenate(const FName Name, const FStringView& String)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	AppendName(Builder, Name);
	Builder.Append(String);
	return FName(Builder);
}

FName FVoxelUtilities::Concatenate(const FName A, const FName B)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	AppendName(Builder, A);
	AppendName(Builder, B);
	return FName(Builder);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelUtilities::BlobToHex(const TConstVoxelArrayView64<uint8> Data)
{
	return FString::FromHexBlob(Data.GetData(), Data.Num());
}

TVoxelArray<uint8> FVoxelUtilities::HexToBlob(const FString& Source)
{
	if (!ensure(Source.Len() % 2 == 0))
	{
		return {};
	}

	TVoxelArray<uint8> Result;
	SetNumFast(Result, Source.Len() / 2);

	verify(FString::ToHexBlob(Source, Result.GetData(), Result.Num()));

	return Result;
}