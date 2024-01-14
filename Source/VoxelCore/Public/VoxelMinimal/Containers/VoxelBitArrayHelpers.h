// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

struct VOXELCORE_API FVoxelBitArrayHelpers
{
	static constexpr int32 NumBitsPerWord = 32;

#if VOXEL_DEBUG
#define ShiftOperand(X) CheckShiftOperand(X)
#else
#define ShiftOperand(X) (X)
#endif

	FORCEINLINE static bool IsValidShift(const int32 Value)
	{
		// The shift operand is masked with 5
		return Value >= 0 && Value < 32;
	}
	template<typename T>
	FORCEINLINE static T CheckShiftOperand(T Value)
	{
		checkVoxelSlow(IsValidShift(Value));
		return Value;
	}

	FORCEINLINE static bool Get(const uint32* RESTRICT Data, const uint32 Index)
	{
		const uint32 Mask = 1u << (Index % NumBitsPerWord);
		return Data[Index / NumBitsPerWord] & Mask;
	}
	FORCEINLINE static void Set(uint32* RESTRICT Data, const uint32 Index, const bool bValue)
	{
		const uint32 Mask = 1u << (Index % NumBitsPerWord);
		uint32& Value = Data[Index / NumBitsPerWord];

		if (bValue)
		{
			Value |= Mask;
		}
		else
		{
			Value &= ~Mask;
		}

		checkVoxelSlow(Get(Data, Index) == bValue);
	}

	template<typename ArrayType, typename = decltype(GetNum(DeclVal<ArrayType>()))>
	FORCEINLINE static bool Get(const ArrayType& Array, const int32 Index)
	{
		checkVoxelSlow(0 <= Index && Index < GetNum(Array) * NumBitsPerWord);
		return FVoxelBitArrayHelpers::Get(GetData(Array), Index);
	}
	template<typename ArrayType, typename = decltype(GetNum(DeclVal<ArrayType>()))>
	FORCEINLINE static void Set(const ArrayType& Array, const int32 Index, const bool bValue)
	{
		checkVoxelSlow(0 <= Index && Index < GetNum(Array) * NumBitsPerWord);
		FVoxelBitArrayHelpers::Set(GetData(Array), Index, bValue);
	}

	template<typename T>
	FORCEINLINE static bool TestAndClear(T& Array, int32 Index)
	{
		checkVoxelSlow(0 <= Index && Index < Array.Num());

		const uint32 Mask = 1u << (Index % NumBitsPerWord);
		uint32& Value = Array.GetWordData()[Index / NumBitsPerWord];

		const bool bResult = Value & Mask;
		checkVoxelSlow(Array[Index] == bResult);
		Value &= ~Mask;
		checkVoxelSlow(!Array[Index]);
		return bResult;
	}

public:
	template<typename T>
	FORCEINLINE static bool TestRange(const T& Array, const int32 Index, const int32 Num)
	{
		checkVoxelSlow(0 <= Index && Index + Num <= Array.Num() && Num > 0);
		const bool bResult = TestRangeImpl(Array.GetWordData(), Index, Num);

#if VOXEL_DEBUG
		if (bResult)
		{
			for (int32 It = Index; It < Index + Num; It++)
			{
				check(Array[It]);
			}
		}
		else
		{
			bool bValue = true;
			for (int32 It = Index; It < Index + Num; It++)
			{
				bValue &= Array[It];
			}
			check(!bValue);
		}
#endif

		return bResult;
	}

	static bool TestRangeImpl(const uint32* RESTRICT ArrayData, int32 Index, int32 Num);

public:
	// Test a range, and clears it if all true
	template<typename T>
	FORCEINLINE static bool TestAndClearRange(T& Array, const int32 Index, const int32 Num)
	{
		checkVoxelSlow(0 <= Index && Index + Num <= Array.Num() && Num > 0);
		return TestAndClearRangeImpl(Array.GetWordData(), Index, Num);
	}

	static bool TestAndClearRangeImpl(uint32* RESTRICT ArrayData, int32 Index, int32 Num);

public:
	template<typename T>
	static TOptional<bool> TryGetAll(const T& Array)
	{
		const TOptional<bool> Result = TryGetAllImpl(Array);

#if VOXEL_DEBUG
		if (Result.IsSet())
		{
			for (int32 Index = 0; Index < Array.Num(); Index++)
			{
				check(Result.GetValue() == Array[Index]);
			}
		}
		else if (Array.Num() > 0)
		{
			bool bFound = false;
			for (int32 Index = 1; Index < Array.Num(); Index++)
			{
				if (Array[Index] != Array[0])
				{
					bFound = true;
					break;
				}
			}
			check(bFound);
		}
#endif

		return Result;
	}
	template<typename T>
	static TOptional<bool> TryGetAllImpl(const T& Array)
	{
		if (Array.Num() == 0)
		{
			return {};
		}

		const bool bValue = Get(Array.GetWordData(), 0);
		if (AllEqual(Array, bValue))
		{
			return bValue;
		}
		else
		{
			return {};
		}
	}
	template<typename T>
	static bool AllEqual(const T& Array, bool bValue)
	{
		const bool bResult = bValue ? AllEqualImpl<true>(Array) : AllEqualImpl<false>(Array);

#if VOXEL_DEBUG
		bool bActualResult = true;
		for (int32 Index = 0; Index < Array.Num(); Index++)
		{
			bActualResult &= (Array[Index] == bValue);
		}
		check(bResult == bActualResult);
#endif

		return bResult;
	}
	template<bool bValue, typename T>
	static bool AllEqualImpl(const T& Array)
	{
		if (Array.Num() == 0)
		{
			return true;
		}

		const uint32* RESTRICT Data = Array.GetWordData();

		const int32 End = FVoxelUtilities::DivideFloor_Positive(Array.Num(), NumBitsPerWord);
		for (int32 Index = 0; Index < End; Index++)
		{
			if (bValue && Data[Index] != uint32(-1))
			{
				return false;
			}
			if (!bValue && Data[Index] != 0)
			{
				return false;
			}
		}

		if (End * NumBitsPerWord != Array.Num())
		{
			const int32 NumBitsInLastWord = Array.Num() - End * NumBitsPerWord;
			checkVoxelSlow(NumBitsInLastWord > 0);
			checkVoxelSlow(NumBitsInLastWord < NumBitsPerWord);

			const uint32 LastWord = Data[End];
			const uint32 Mask = (1 << NumBitsInLastWord) - 1;

			if (bValue && (LastWord & Mask) != Mask)
			{
				return false;
			}
			if (!bValue && (LastWord & Mask) != 0)
			{
				return false;
			}
		}

		return true;
	}

public:
	FORCEINLINE static uint32 GetPackedSafe(
		const int32 TypeSizeInBits,
		const uint32* RESTRICT ArrayData,
		const uint32* RESTRICT MaxArrayData,
		const int64 Index)
	{
		return GetOrSetPacked<false>(TypeSizeInBits, nullptr, ArrayData, MaxArrayData, Index, 0);
	}
	FORCEINLINE static void SetPackedSafe(
		const int32 TypeSizeInBits,
		uint32* RESTRICT ArrayData,
		const uint32* RESTRICT MaxArrayData,
		const int64 Index,
		const uint32 Value)
	{
		GetOrSetPacked<true>(TypeSizeInBits, ArrayData, nullptr, MaxArrayData, Index, Value);
	}

	FORCEINLINE static uint32 GetPacked(
		const int32 TypeSizeInBits,
		const uint32* RESTRICT ArrayData,
		const int64 Index)
	{
		return GetPackedSafe(TypeSizeInBits, ArrayData, reinterpret_cast<const uint32*>(-1), Index);
	}
	FORCEINLINE static void SetPacked(
		const int32 TypeSizeInBits,
		uint32* RESTRICT ArrayData,
		const int64 Index,
		const uint32 Value)
	{
		SetPackedSafe(TypeSizeInBits, ArrayData, reinterpret_cast<const uint32*>(-1), Index, Value);
	}

	template<typename DataType>
	FORCEINLINE static typename TEnableIf<!TIsPODType<DataType>::Value, uint32>::Type GetPacked(
		const int32 TypeSizeInBits,
		const DataType& Data,
		const int64 Index)
	{
		// eg, uint8
		checkStatic(TIsPODType<VOXEL_GET_TYPE(*::GetData(Data))>::Value);

		auto* DataPtr = GetData(Data);
		auto* MaxDataPtr = DataPtr + GetNum(Data);

		return GetPackedSafe(TypeSizeInBits, reinterpret_cast<const uint32*>(DataPtr), reinterpret_cast<const uint32*>(MaxDataPtr), Index);
	}
	template<typename DataType>
	FORCEINLINE static typename TEnableIf<!TIsPODType<DataType>::Value>::Type SetPacked(
		const int32 TypeSizeInBits,
		DataType& Data,
		const int64 Index,
		const uint32 Value)
	{
		// eg, uint8
		checkStatic(TIsPODType<VOXEL_GET_TYPE(*::GetData(Data))>::Value);

		auto* DataPtr = GetData(Data);
		auto* MaxDataPtr = DataPtr + GetNum(Data);

		SetPackedSafe(TypeSizeInBits, reinterpret_cast<uint32*>(DataPtr), reinterpret_cast<uint32*>(MaxDataPtr), Index, Value);
	}

	template<bool bSet>
	FORCEINLINE static uint32 GetOrSetPacked(
		const int32 TypeSizeInBits,
		uint32* RESTRICT MutableArrayData,
		const uint32* RESTRICT ConstArrayData,
		const uint32* RESTRICT MaxArrayData,
		const int64 Index,
		const uint32 ValueToSet)
	{
		checkVoxelSlow(0 < TypeSizeInBits && TypeSizeInBits <= NumBitsPerWord);
		checkVoxelSlow(int32(FMath::CeilLogTwo(ValueToSet)) <= TypeSizeInBits);

		const int64 StartBit = Index * TypeSizeInBits;
		const int64 EndBit = (Index + 1) * TypeSizeInBits;

		const int64 StartIndex = StartBit / NumBitsPerWord;
		const int64 Count = FVoxelUtilities::DivideCeil_Positive<int64>(EndBit, NumBitsPerWord) - StartIndex;
		checkVoxelSlow(Count == 1 || Count == 2);

		const int32 StartBitInWord = StartBit % NumBitsPerWord;
		const int32 EndBitInWord = EndBit % NumBitsPerWord;

		// Work out masks for the start/end of the sequence
		const uint32 StartMask = 0xFFFFFFFFu << StartBitInWord;
		// eg, if we want to set the 8 first bits, we need to offset the mask by 32 - 8 = 24
		// Second % needed to handle the case where EndBitInWord = 0 (then we want the mask to be all 0xFF)
		const uint32 EndMask = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - EndBitInWord) % NumBitsPerWord);

		uint32* RESTRICT MutableData = MutableArrayData + StartIndex;
		const uint32* RESTRICT ConstData = ConstArrayData + StartIndex;

		if (Count == 1)
		{
			if (bSet)
			{
				checkVoxelSlow(MutableData < MaxArrayData);
				*MutableData &= ~(StartMask & EndMask);
				*MutableData |= ValueToSet << StartBitInWord;

				return 0;
			}
			else
			{
				checkVoxelSlow(ConstData < MaxArrayData);
				return (*ConstData & StartMask & EndMask) >> StartBitInWord;
			}
		}
		else
		{
			checkVoxelSlow(Count == 2);

			const int32 NumBitsInFirstWord = NumBitsPerWord - StartBitInWord;
			checkVoxelSlow(NumBitsInFirstWord < TypeSizeInBits);

			if (bSet)
			{
				checkVoxelSlow(MutableData + 1 < MaxArrayData);

				MutableData[0] &= ~StartMask;
				MutableData[0] |= ValueToSet << StartBitInWord;
				MutableData[1] &= ~EndMask;
				MutableData[1] |= ValueToSet >> ShiftOperand(NumBitsInFirstWord);

				return 0;
			}
			else
			{
				checkVoxelSlow(ConstData + 1 < MaxArrayData);

				uint32 Value = 0;
				Value |= (ConstData[0] & StartMask) >> StartBitInWord;
				Value |= (ConstData[1] & EndMask) << ShiftOperand(NumBitsInFirstWord);
				return Value;
			}
		}
	}

public:
	FORCEINLINE static void Copy(
		uint32* RESTRICT DstArrayData,
		const uint32* RESTRICT SrcArrayData,
		const int64 DstStartBit,
		const int64 SrcStartBit,
		const int64 Num)
	{
#if VOXEL_DEBUG
		const int32 NumBytes = FVoxelUtilities::DivideCeil_Positive<int32>(DstStartBit + Num, 32) * 4;
		uint32* Temp = static_cast<uint32*>(FMemory::Malloc(NumBytes + 8));
		FMemory::Memcpy(Temp, DstArrayData, NumBytes);

		for (int32 Index = 0; Index < Num; Index++)
		{
			Set(Temp, DstStartBit + Index, Get(SrcArrayData, SrcStartBit + Index));
		}

		CopyImpl(DstArrayData, SrcArrayData, DstStartBit, SrcStartBit, Num);

		check(FVoxelUtilities::MemoryEqual(DstArrayData, Temp, NumBytes));
		FMemory::Free(Temp);
#else
		CopyImpl(DstArrayData, SrcArrayData, DstStartBit, SrcStartBit, Num);
#endif
	}

	static void CopyImpl(
		uint32* RESTRICT DstArrayData,
		const uint32* RESTRICT SrcArrayData,
		int64 DstStartBit,
		int64 SrcStartBit,
		int64 Num);

public:
	FORCEINLINE static bool Equal(
		const uint32* RESTRICT ArrayDataA,
		const uint32* RESTRICT ArrayDataB,
		const int64 StartBitA,
		const int64 StartBitB,
		const int64 Num)
	{
		const bool bResult = EqualImpl(ArrayDataA, ArrayDataB, StartBitA, StartBitB, Num);
#if VOXEL_DEBUG
		const auto Naive = [&]
		{
			for (int32 Index = 0; Index < Num; Index++)
			{
				if (Get(ArrayDataA, StartBitA + Index) != Get(ArrayDataB, StartBitB + Index))
				{
					return false;
				}
			}
			return true;
		};
		check(bResult == Naive());
#endif
		return bResult;
	}

	static bool EqualImpl(
		const uint32* RESTRICT ArrayDataA,
		const uint32* RESTRICT ArrayDataB,
		int64 StartBitA,
		int64 StartBitB,
		int64 Num);

public:
	template<typename T>
	FORCEINLINE static void SetRange(T& Array, const int32 Index, const int32 Num, const bool bValue)
	{
		checkVoxelSlow(0 <= Index && Index + Num <= Array.Num());
		SetRange(Array.GetWordData(), Index, Num, bValue);
	}

	static void SetRange(uint32* RESTRICT Data, int32 Index, int32 Num, bool bValue);

public:
	template<typename WordType, typename LambdaType>
	FORCEINLINE static bool ForAllSetBitsInWord(WordType Word, const int32 WordIndex, const int32 Num, LambdaType Lambda)
	{
		constexpr int32 NumBitsPerWordInLoop = sizeof(WordType) * 8;

		if (!Word)
		{
			return false;
		}

		int32 Index = WordIndex * NumBitsPerWordInLoop;

		do
		{
			const uint32 IndexInShiftedWord = FVoxelUtilities::FirstBitLow(Word);
			Index += IndexInShiftedWord;
			Word >>= IndexInShiftedWord;

			// Check multiple bits at once in case they're all 1, critical for high throughput in dense words
			constexpr int32 NumBitsToCheck = 4;
			constexpr int32 BitsCheckMask = (1 << NumBitsToCheck) - 1;
			if ((Word & BitsCheckMask) == BitsCheckMask)
			{
				for (int32 BitIndex = 0; BitIndex < NumBitsToCheck; BitIndex++)
				{
					// Last word should be masked properly
					checkVoxelSlow(Index < Num);

					if constexpr (std::is_same_v<decltype(Lambda(DeclVal<int32>())), void>)
					{
						Lambda(Index);
					}
					else
					{
						if (!Lambda(Index))
						{
							return true;
						}
					}

					Index++;
				}
				Word >>= NumBitsToCheck;
				continue;
			}

			// Last word should be masked properly
			checkVoxelSlow(Index < Num);

			if constexpr (std::is_same_v<decltype(Lambda(DeclVal<int32>())), void>)
			{
				Lambda(Index);
			}
			else
			{
				if (!Lambda(Index))
				{
					return true;
				}
			}

			Word >>= 1;
			Index += 1;
		}
		while (Word);

		return false;
	}

	template<typename LambdaType>
	FORCEINLINE static void ForAllSetBits(
		const uint32* RESTRICT Data,
		const int32 NumWords,
		const int32 Num,
		LambdaType Lambda)
	{
		const uint64* RESTRICT DataIt = reinterpret_cast<const uint64*>(Data);
		const uint64* RESTRICT DataEnd = DataIt + FVoxelUtilities::DivideFloor_Positive<int32>(NumWords, 2);

		while (DataIt != DataEnd)
		{
			uint64 Word = *DataIt;
			if (ForAllSetBitsInWord(Word, DataIt - reinterpret_cast<const uint64*>(Data), Num, Lambda))
			{
				return;
			}
			DataIt++;
		}

		if (NumWords % 2 == 1)
		{
			uint32 Word = *(Data + NumWords - 1);
			ForAllSetBitsInWord(Word, NumWords - 1, Num, Lambda);
		}
	}

	static int64 CountSetBits(const uint32* RESTRICT Data, int32 NumWords);
	static int64 CountSetBits_UpperBound(const uint32* RESTRICT Data, int32 NumBits);
};