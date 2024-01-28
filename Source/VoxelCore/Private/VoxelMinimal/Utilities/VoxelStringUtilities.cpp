// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FText FVoxelUtilities::ConvertToTimeText(double Value, const int32 NumExtraDigits)
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

FText FVoxelUtilities::ConvertToNumberText(double Value)
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
			}
		}
	}

	return FText::Format(INVTEXT("{0}{1}"), FText::AsNumber(Value, &Options), FText::FromString(Unit));
}

bool FVoxelUtilities::IsInt(const FStringView& Text)
{
	for (const TCHAR& Char : Text)
	{
		if (Char == TEXT('-') ||
			Char == TEXT('+'))
		{
			if (&Char != &Text[0])
			{
				return false;
			}
			continue;
		}

		if (FChar::IsDigit(Char))
		{
			continue;
		}

		return false;
	}

	return true;
}

bool FVoxelUtilities::IsFloat(const FStringView& Text)
{
	for (const TCHAR Char : Text)
	{
		if (Char == TEXT('-') ||
			Char == TEXT('+'))
		{
			if (&Char != &Text[0])
			{
				return false;
			}
			continue;
		}

		if (Char == TEXT('.') ||
			Char == TEXT('f') ||
			FChar::IsDigit(Char))
		{
			continue;
		}

		return false;
	}

	return true;
}

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