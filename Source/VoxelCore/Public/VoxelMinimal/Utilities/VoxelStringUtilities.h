// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

class FJsonValue;
class FJsonObject;
enum class EUnit : uint8;

namespace FVoxelUtilities
{
	VOXELCORE_API FString JsonToString(
		const TSharedRef<FJsonObject>& JsonObject,
		bool bPrettyPrint = false);

	VOXELCORE_API TSharedPtr<FJsonObject> StringToJson(const FString& String);
	VOXELCORE_API TSharedPtr<FJsonValue> StringToJsonValue(const FString& String);

	VOXELCORE_API FText SecondsToText(double Value, int32 NumExtraDigits = 0);
	VOXELCORE_API FText BytesToText(double Value);
	VOXELCORE_API FText NumberToText(double Value, int32 NumSignificantNumbers = 4);
	VOXELCORE_API FString DistanceToString(double DistanceInCentimeters, int32 NumSignificantNumbers, EUnit& OutUnit, int32& OutNumFractionalDigits);
	VOXELCORE_API FString DistanceToString(double DistanceInCentimeters, int32 NumSignificantNumbers = 4);

	VOXELCORE_API FString SecondsToString(double Value, int32 NumExtraDigits = 0);
	VOXELCORE_API FString BytesToString(double Value);
	VOXELCORE_API FString NumberToString(double Value, int32 NumSignificantNumbers = 4);

	VOXELCORE_API bool IsInt(FStringView Text);
	VOXELCORE_API bool IsFloat(FStringView Text);

	FORCEINLINE int64 StringToInt(const FStringView& Text)
	{
		ensureVoxelSlowNoSideEffects(IsInt(Text));
		return FCString::Atoi64(Text.GetData());
	}
	FORCEINLINE float StringToFloat(const FStringView& Text)
	{
		ensureVoxelSlowNoSideEffects(IsFloat(Text));
		return FCString::Atof(Text.GetData());
	}
	FORCEINLINE double StringToDouble(const FStringView& Text)
	{
		ensureVoxelSlowNoSideEffects(IsFloat(Text));
		return FCString::Atod(Text.GetData());
	}

	VOXELCORE_API FName Concatenate(const FStringView& String, FName Name);
	VOXELCORE_API FName Concatenate(FName Name, const FStringView& String);
	VOXELCORE_API FName Concatenate(FName A, FName B);

	VOXELCORE_API FString BlobToHex(TConstVoxelArrayView64<uint8> Data);
	VOXELCORE_API TVoxelArray<uint8> HexToBlob(const FString& Source);

	template<typename BuilderType, typename NumberType>
	requires std::is_integral_v<NumberType>
	FORCEINLINE void AppendNumber(BuilderType& Builder, const NumberType Number)
	{
		constexpr int32 LocalBufferSize = CeilLog10<TNumericLimits<NumberType>::Max()>();
		TCHAR LocalBuffer[LocalBufferSize];

		NumberType LocalNumber = FMath::Abs(Number);
		int32 LocalIndex = LocalBufferSize - 1;
		while (true)
		{
			const TCHAR* DigitToChar = TEXT("0123456789");

			checkVoxelSlow(0 <= LocalIndex && LocalIndex < LocalBufferSize);
			LocalBuffer[LocalIndex] = DigitToChar[LocalNumber % 10];

			LocalNumber /= 10;

			if (LocalNumber == 0)
			{
				break;
			}

			LocalIndex--;
		}

		if (Number < 0)
		{
			Builder.AppendChar(TEXT('-'));
		}

		Builder.Append(FStringView(&LocalBuffer[LocalIndex], LocalBufferSize - LocalIndex));
	}
	template<typename BuilderType>
	void AppendName(BuilderType& Builder, const FName Name)
	{
		Name.GetDisplayNameEntry()->AppendNameToString(Builder);

		const int32 InternalNumber = Name.GetNumber();
		if (InternalNumber == NAME_NO_NUMBER_INTERNAL)
		{
			return;
		}

		Builder.AppendChar('_');
		AppendNumber(Builder, NAME_INTERNAL_TO_EXTERNAL(InternalNumber));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API FString LexToString(const FSphere& Sphere);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FName operator+(const FStringView& A, const FName B)
{
	return FVoxelUtilities::Concatenate(A, B);
}
FORCEINLINE FName operator+(const FName A, const FStringView& B)
{
	return FVoxelUtilities::Concatenate(A, B);
}
FORCEINLINE FName operator+(const FName A, const FName B)
{
	return FVoxelUtilities::Concatenate(A, B);
}
template<typename T>
FName operator+(FName, T*) = delete;

FORCEINLINE FName& operator+=(FName& A, const FStringView& B)
{
	A = A + B;
	return A;
}
FORCEINLINE FName& operator+=(FName& A, const FName B)
{
	A = A + B;
	return A;
}
template<typename T>
FName& operator+=(FName&, T*) = delete;