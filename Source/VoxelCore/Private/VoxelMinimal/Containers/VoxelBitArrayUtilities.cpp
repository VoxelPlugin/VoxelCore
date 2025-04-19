// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

bool FVoxelBitArrayUtilities::AllEqual(
	const TConstVoxelArrayView<uint32> Words,
	const int32 NumBits,
	bool bValue)
{
	VOXEL_FUNCTION_COUNTER_NUM(NumBits, 4096);

	const int32 NumWords = FVoxelUtilities::DivideFloor_Positive(NumBits, NumBitsPerWord);

	if (!FVoxelUtilities::AllEqual(Words.LeftOf(NumWords), bValue ? FullWord : EmptyWord))
	{
		return false;
	}

	if (NumWords * NumBitsPerWord != NumBits)
	{
		const int32 NumBitsInLastWord = NumBits - NumWords * NumBitsPerWord;
		checkVoxelSlow(NumBitsInLastWord > 0);
		checkVoxelSlow(NumBitsInLastWord < NumBitsPerWord);

		const uint32 LastWord = Words[NumWords];
		const uint32 Mask = (1u << NumBitsInLastWord) - 1;

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

TVoxelOptional<bool> FVoxelBitArrayUtilities::TryGetAll(
	const TConstVoxelArrayView<uint32> Words,
	const int32 NumBits)
{
	if (NumBits == 0)
	{
		return {};
	}

	const bool bValue = Get(Words, 0);

	if (!AllEqual(Words, NumBits, bValue))
	{
		return {};
	}

	return bValue;
}

void FVoxelBitArrayUtilities::SetRange(
	const TVoxelArrayView<uint32> Data,
	const int32 StartIndex,
	const int32 Num,
	const bool bValue)
{
	if (Num == 0)
	{
		return;
	}

#if VOXEL_DEBUG
	TVoxelArray<uint32> DataCopy = Data.Array();

	for (int32 Index = 0; Index < Num; Index++)
	{
		Set(DataCopy, StartIndex + Index, bValue);
	}

	ON_SCOPE_EXIT
	{
		check(DataCopy == Data);
	};
#endif

	// Work out which uint32 index to set from, and how many
	const int32 FirstWordIndex = FVoxelUtilities::DivideFloor_Positive(StartIndex, NumBitsPerWord);
	const int32 LastWordIndex = FVoxelUtilities::DivideFloor_Positive(StartIndex + Num - 1, NumBitsPerWord);

	const int32 NumWords = 1 + LastWordIndex - FirstWordIndex;
	checkVoxelSlow(NumWords >= 1);

	// Work out masks for the start/end of the sequence
	const uint32 StartMask = 0xFFFFFFFFu << (StartIndex & IndexInWordMask);
	// Second % needed to handle the case where (Index + Num) % NumBitsPerWord = 0
	const uint32 EndMask = 0xFFFFFFFFu >> ((NumBitsPerWord - (StartIndex + Num) & IndexInWordMask) & IndexInWordMask);

	if (bValue)
	{
		if (NumWords == 1)
		{
			Data[FirstWordIndex] |= StartMask & EndMask;
		}
		else
		{
			Data[FirstWordIndex] |= StartMask;

			if (NumWords > 2)
			{
				FVoxelUtilities::Memset(Data.Slice(FirstWordIndex + 1, NumWords - 2), 0xFF);
			}

			Data[LastWordIndex] |= EndMask;
		}
	}
	else
	{
		if (NumWords == 1)
		{
			Data[FirstWordIndex] &= ~(StartMask & EndMask);
		}
		else
		{
			Data[FirstWordIndex] &= ~StartMask;

			if (NumWords > 2)
			{
				FVoxelUtilities::Memset(Data.Slice(FirstWordIndex + 1, NumWords - 2), 0);
			}

			Data[LastWordIndex] &= ~EndMask;
		}
	}
}