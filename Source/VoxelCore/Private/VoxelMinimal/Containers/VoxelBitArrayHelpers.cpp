// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"

bool FVoxelBitArrayHelpers::TestRangeImpl(const uint32* RESTRICT ArrayData, const int32 Index, const int32 Num)
{
	// Work out which uint32 index to set from, and how many
	const int32 StartIndex = Index / NumBitsPerWord;
	int32 Count = FVoxelUtilities::DivideCeil_Positive(Index + Num, NumBitsPerWord) - StartIndex;

	// Work out masks for the start/end of the sequence
	const uint32 StartMask = 0xFFFFFFFFu << (Index % NumBitsPerWord);
	// Second % needed to handle the case where (Index + Num) % NumBitsPerWord = 0 (then we want the mask to be all 0xFF)
	const uint32 EndMask   = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - (Index + Num) % NumBitsPerWord) % NumBitsPerWord);

	const uint32* RESTRICT Data = ArrayData + StartIndex;

	if (Count == 1)
	{
		const uint32 Mask = StartMask & EndMask;
		if ((*Data & Mask) != Mask)
		{
			return false;
		}
	}
	else
	{
		if ((*Data++ & StartMask) != StartMask)
		{
			return false;
		}

		Count -= 2;
		while (Count != 0)
		{
			if ((*Data++ & ~0) != ~0)
			{
				return false;
			}
			--Count;
		}

		if ((*Data & EndMask) != EndMask)
		{
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelBitArrayHelpers::TestAndClearRangeImpl(uint32* RESTRICT ArrayData, const int32 Index, const int32 Num)
{
	// Work out which uint32 index to set from, and how many
	const int32 StartIndex = Index / NumBitsPerWord;
	const int32 Count = FVoxelUtilities::DivideCeil_Positive(Index + Num, NumBitsPerWord) - StartIndex;

	// Work out masks for the start/end of the sequence
	const uint32 StartMask = 0xFFFFFFFFu << (Index % NumBitsPerWord);
	// Second % needed to handle the case where (Index + Num) % NumBitsPerWord = 0 (then we want the mask to be all 0xFF)
	const uint32 EndMask   = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - (Index + Num) % NumBitsPerWord) % NumBitsPerWord);

	uint32* RESTRICT const Data = ArrayData + StartIndex;

	if (Count == 1)
	{
		const uint32 Mask = StartMask & EndMask;
		if ((*Data & Mask) != Mask)
		{
			return false;
		}

		*Data &= ~Mask;
		return true;
	}
	else
	{
		{
			uint32* RESTRICT LocalData = Data;
			int32 LocalCount = Count;

			if ((*LocalData++ & StartMask) != StartMask)
			{
				return false;
			}

			LocalCount -= 2;
			while (LocalCount != 0)
			{
				if (*LocalData++ != 0xFFFFFFFFu)
				{
					return false;
				}
				--LocalCount;
			}

			if ((*LocalData & EndMask) != EndMask)
			{
				return false;
			}
		}

		{
			uint32* RESTRICT LocalData = Data;
			int32 LocalCount = Count;

			*LocalData++ &= ~StartMask;
			LocalCount -= 2;
			while (LocalCount != 0)
			{
				*LocalData++ = 0;
				--LocalCount;
			}
			*LocalData &= ~EndMask;

			return true;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBitArrayHelpers::CopyImpl(
	uint32* RESTRICT DstArrayData,
	const uint32* RESTRICT SrcArrayData,
	const int64 DstStartBit,
	const int64 SrcStartBit,
	const int64 Num)
{
	const int64 DstEndBit = DstStartBit + Num;
	const int64 SrcEndBit = SrcStartBit + Num;

	const uint32 DstStartBitInWord = DstStartBit % NumBitsPerWord;
	const uint32 SrcStartBitInWord = SrcStartBit % NumBitsPerWord;

	const uint32 DstEndBitInWord = DstEndBit % NumBitsPerWord;
	const uint32 SrcEndBitInWord = SrcEndBit % NumBitsPerWord;

	const uint64 DstStartIndex = DstStartBit / NumBitsPerWord;
	const uint64 SrcStartIndex = SrcStartBit / NumBitsPerWord;

	// Work out masks for the start/end of the sequence
	const uint32 DstStartMask = 0xFFFFFFFFu << DstStartBitInWord;
	const uint32 SrcStartMask = 0xFFFFFFFFu << SrcStartBitInWord;
	// eg, if we want to set the 8 first bits, we need to offset the mask by 32 - 8 = 24
	// % needed to handle the case where EndBitInWord = 0
	const uint32 DstEndMask = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - DstEndBitInWord) % NumBitsPerWord);
	const uint32 SrcEndMask = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - SrcEndBitInWord) % NumBitsPerWord);

	const int64 DstCount = FVoxelUtilities::DivideCeil_Positive<int64>(DstEndBit, NumBitsPerWord) - DstStartIndex;
	const int64 SrcCount = FVoxelUtilities::DivideCeil_Positive<int64>(SrcEndBit, NumBitsPerWord) - SrcStartIndex;

	uint32* RESTRICT DstData = DstArrayData + DstStartIndex;
	const uint32* RESTRICT SrcData = SrcArrayData + SrcStartIndex;

	if (DstStartBitInWord == SrcStartBitInWord)
	{
		// Aligned copy, faster

		checkVoxelSlow(DstCount == SrcCount);
		checkVoxelSlow(DstStartMask == SrcStartMask);
		checkVoxelSlow(DstEndMask == SrcEndMask);

		const uint64 Count = DstCount;
		const uint32 StartMask = DstStartMask;
		const uint32 EndMask = DstEndMask;

		if (Count == 1)
		{
			DstData[0] &= ~(StartMask & EndMask);
			DstData[0] |= (SrcData[0] & StartMask & EndMask);
		}
		else if (Count == 2)
		{
			DstData[0] &= ~StartMask;
			DstData[0] |= SrcData[0] & StartMask;
			DstData[1] &= ~EndMask;
			DstData[1] |= SrcData[1] & EndMask;
		}
		else
		{
			DstData[0] &= ~StartMask;
			DstData[0] |= SrcData[0] & StartMask;

			FMemory::Memcpy(DstData + 1, SrcData + 1, sizeof(uint32) * (Count - 2));

			DstData[Count - 1] &= ~EndMask;
			DstData[Count - 1] |= SrcData[Count - 1] & EndMask;
		}
	}
	else
	{
		if (Num <= NumBitsPerWord)
		{
			checkVoxelSlow(DstCount == 1 || DstCount == 2);
			checkVoxelSlow(SrcCount == 1 || SrcCount == 2);

			uint32 SrcWord;

			if (SrcCount == 1)
			{
				// SrcStartMask not needed
				SrcWord = (SrcData[0] & SrcEndMask) >> SrcStartBitInWord;
			}
			else
			{
				SrcWord = SrcData[0] >> SrcStartBitInWord;

				checkVoxelSlow(SrcStartBitInWord != 0); // Otherwise, SrcCount should be 1
				SrcWord |= SrcData[1] << ShiftOperand(NumBitsPerWord - SrcStartBitInWord);
			}

			if (DstCount == 1)
			{
				DstData[0] &= ~(DstStartMask & DstEndMask);
				// No need to apply DstStartMask
				DstData[0] |= ((SrcWord << DstStartBitInWord) & DstEndMask);
			}
			else
			{
				checkVoxelSlow(DstCount == 2);

				DstData[0] &= ~DstStartMask;
				DstData[0] |= SrcWord << DstStartBitInWord;

				checkVoxelSlow(DstStartBitInWord != 0); // Otherwise, DstCount should be 1
				DstData[1] &= ~DstEndMask;
				DstData[1] |= DstEndMask & (SrcWord >> ShiftOperand(NumBitsPerWord - DstStartBitInWord));
			}
		}
		else
		{
			checkVoxelSlow(DstCount >= 2);
			checkVoxelSlow(SrcCount >= 2);
			checkVoxelSlow(FMath::Abs(DstCount - SrcCount) <= 2);

			uint32 SrcFirstWord = SrcData[0] >> SrcStartBitInWord;

			if (SrcStartBitInWord != 0)
			{
				SrcFirstWord |= SrcData[1] << ShiftOperand(NumBitsPerWord - SrcStartBitInWord);
			}

			DstData[0] &= ~DstStartMask;
			DstData[0] |= SrcFirstWord << DstStartBitInWord;

			const int32 DstCountToCopy = DstCount - (DstEndBitInWord != 0);

			// If true, src is one word forward: use SrcWord0 to make DstWord0, SrcWord0 and SrcWord1 to make DstWord1 etc
			// If false, use SrcWord0 and SrcWord1 to make DstWord0
			if (DstStartBitInWord > SrcStartBitInWord)
			{
				const int32 BitOffset = NumBitsPerWord - (DstStartBitInWord - SrcStartBitInWord);
				for (int64 Index = 1; Index < DstCountToCopy; Index++)
				{
					checkVoxelSlow(Index < SrcCount);
					DstData[Index] = (SrcData[Index - 1] >> ShiftOperand(BitOffset)) | (SrcData[Index] << ShiftOperand(NumBitsPerWord - BitOffset));
				}
			}
			else
			{
				// Branch handled above
				checkVoxelSlow(DstStartBitInWord != SrcStartBitInWord);

				const int32 BitOffset = SrcStartBitInWord - DstStartBitInWord;
				for (int64 Index = 1; Index < DstCountToCopy; Index++)
				{
					checkVoxelSlow(Index + 1 < SrcCount);
					DstData[Index] = (SrcData[Index] >> ShiftOperand(BitOffset)) | (SrcData[Index + 1] << ShiftOperand(NumBitsPerWord - BitOffset));
				}
			}

			// Copy the last word manually if not aligned
			if (DstEndBitInWord != 0)
			{
				// Might not be aligned with SrcFirstWord!
				uint32 SrcLastWord;
				if (SrcEndBitInWord == 0)
				{
					SrcLastWord = SrcData[SrcCount - 1];
				}
				else
				{
					SrcLastWord = SrcData[SrcCount - 2] >> ShiftOperand(SrcEndBitInWord);
					SrcLastWord |= SrcData[SrcCount - 1] << ShiftOperand(NumBitsPerWord - SrcEndBitInWord);
				}

				DstData[DstCount - 1] &= ~DstEndMask;
				DstData[DstCount - 1] |= SrcLastWord >> ShiftOperand(NumBitsPerWord - DstEndBitInWord);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelBitArrayHelpers::EqualImpl(
	const uint32* RESTRICT ArrayDataA,
	const uint32* RESTRICT ArrayDataB,
	const int64 StartBitA,
	const int64 StartBitB,
	const int64 Num)
{
	const int64 EndBitA = StartBitA + Num;
	const int64 EndBitB = StartBitB + Num;

	const uint32 StartBitInWordA = StartBitA % NumBitsPerWord;
	const uint32 StartBitInWordB = StartBitB % NumBitsPerWord;

	const uint32 EndBitInWordA = EndBitA % NumBitsPerWord;
	const uint32 EndBitInWordB = EndBitB % NumBitsPerWord;

	const uint64 StartIndexA = StartBitA / NumBitsPerWord;
	const uint64 StartIndexB = StartBitB / NumBitsPerWord;

	// Work out masks for the start/end of the sequence
	const uint32 StartMaskA = 0xFFFFFFFFu << StartBitInWordA;
	const uint32 StartMaskB = 0xFFFFFFFFu << StartBitInWordB;
	// eg, if we want to set the 8 first bits, we need to offset the mask by 32 - 8 = 24
	// % needed to handle the EndBitInWord = 0 case
	const uint32 EndMaskA = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - EndBitInWordA) % NumBitsPerWord);
	const uint32 EndMaskB = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - EndBitInWordB) % NumBitsPerWord);

	const int64 CountA = FVoxelUtilities::DivideCeil_Positive<int64>(EndBitA, NumBitsPerWord) - StartIndexA;
	const int64 CountB = FVoxelUtilities::DivideCeil_Positive<int64>(EndBitB, NumBitsPerWord) - StartIndexB;

	const uint32* RESTRICT DataA = ArrayDataA + StartIndexA;
	const uint32* RESTRICT DataB = ArrayDataB + StartIndexB;

	if (StartBitInWordA == StartBitInWordB)
	{
		// Aligned, faster

		checkVoxelSlow(CountA == CountB);
		checkVoxelSlow(StartMaskA == StartMaskB);
		checkVoxelSlow(EndMaskA == EndMaskB);

		const uint64 Count = CountA;
		const uint32 StartMask = StartMaskA;
		const uint32 EndMask = EndMaskA;

		if (Count == 1)
		{
			return (DataA[0] & StartMask & EndMask) == (DataB[0] & StartMask & EndMask);
		}
		else if (Count == 2)
		{
			return
					(DataA[0] & StartMask) == (DataB[0] & StartMask) &&
					(DataA[1] & EndMask) == (DataB[1] & EndMask);
		}
		else
		{
			if ((DataA[0] & StartMask) != (DataB[0] & StartMask))
			{
				return false;
			}
			if ((DataA[Count - 1] & EndMask) != (DataB[Count - 1] & EndMask))
			{
				return false;
			}

			return FVoxelUtilities::MemoryEqual(DataA + 1, DataB + 1, sizeof(uint32) * (Count - 2));
		}
	}
	else
	{
		if (Num <= NumBitsPerWord)
		{
			checkVoxelSlow(CountA == 1 || CountA == 2);
			checkVoxelSlow(CountB == 1 || CountB == 2);

			uint32 WordB;

			if (CountB == 1)
			{
				// SrcStartMask not needed
				WordB = (DataB[0] & EndMaskB) >> StartBitInWordB;
			}
			else
			{
				WordB = DataB[0] >> StartBitInWordB;

				checkVoxelSlow(StartBitInWordB != 0); // Otherwise, CountB should be 1
				WordB |= DataB[1] << ShiftOperand(NumBitsPerWord - StartBitInWordB);
			}

			if (CountA == 1)
			{
				// No need to apply DstStartMask
				return (DataA[0] & StartMaskA & EndMaskA) == ((WordB << StartBitInWordA) & EndMaskA);
			}
			else
			{
				checkVoxelSlow(CountA == 2);
				checkVoxelSlow(StartBitInWordA != 0); // Otherwise, CountA should be 1
				checkVoxelSlow(((WordB << StartBitInWordA) & StartMaskA) == (WordB << StartBitInWordA));

				return
						((DataA[0] & StartMaskA) == (WordB << StartBitInWordA)) &&
						((DataA[1] & EndMaskA) == ((WordB >> ShiftOperand(NumBitsPerWord - StartBitInWordA)) & EndMaskA));
			}
		}
		else
		{
			checkVoxelSlow(CountA >= 2);
			checkVoxelSlow(CountB >= 2);
			checkVoxelSlow(FMath::Abs(CountA - CountB) <= 2);

			uint32 FirstWordB = DataB[0] >> StartBitInWordB;
			if (StartBitInWordB != 0)
			{
				FirstWordB |= DataB[1] << ShiftOperand(NumBitsPerWord - StartBitInWordB);
			}

			if ((DataA[0] & StartMaskA) != (FirstWordB << StartBitInWordA))
			{
				return false;
			}

			const int32 CountAToTest = CountA - (EndBitInWordA != 0);

			// If true, src is one word forward: use SrcWord0 to make DstWord0, SrcWord0 and SrcWord1 to make DstWord1 etc
			// If false, use SrcWord0 and SrcWord1 to make DstWord0
			if (StartBitInWordA > StartBitInWordB)
			{
				const int32 BitOffset = NumBitsPerWord - (StartBitInWordA - StartBitInWordB);
				for (int64 Index = 1; Index < CountAToTest; Index++)
				{
					checkVoxelSlow(Index < CountB);
					if (DataA[Index] != ((DataB[Index - 1] >> ShiftOperand(BitOffset)) | (DataB[Index] << ShiftOperand(NumBitsPerWord - BitOffset))))
					{
						return false;
					}
				}
			}
			else
			{
				const int32 BitOffset = StartBitInWordB - StartBitInWordA;
				for (int64 Index = 1; Index < CountAToTest; Index++)
				{
					checkVoxelSlow(Index + 1 < CountB);
					if (DataA[Index] != ((DataB[Index] >> ShiftOperand(BitOffset)) | (DataB[Index + 1] << ShiftOperand(NumBitsPerWord - BitOffset))))
					{
						return false;
					}
				}
			}

			// Test the last word manually if not aligned
			if (EndBitInWordA != 0)
			{
				// Might not be aligned with FirstWordB!
				uint32 LastWordB;
				if (EndBitInWordB == 0)
				{
					LastWordB = DataB[CountB - 1];
				}
				else
				{
					LastWordB = DataB[CountB - 2] >> ShiftOperand(EndBitInWordB);
					LastWordB |= DataB[CountB - 1] << ShiftOperand(NumBitsPerWord - EndBitInWordB);
				}

				return (DataA[CountA - 1] & EndMaskA) == (LastWordB >> ShiftOperand(NumBitsPerWord - EndBitInWordA));
			}
			else
			{
				return true;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBitArrayHelpers::SetRange(uint32* RESTRICT Data, const int32 Index, const int32 Num, const bool bValue)
{
	if (Num == 0)
	{
		return;
	}

	const uint32* RESTRICT OriginalData = Data;

	// Work out which uint32 index to set from, and how many
	const int32 StartIndex = Index / NumBitsPerWord;
	int32 Count = FVoxelUtilities::DivideCeil_Positive(Index + Num, NumBitsPerWord) - StartIndex;

	// Work out masks for the start/end of the sequence
	const uint32 StartMask = 0xFFFFFFFFu << (Index % NumBitsPerWord);
	// Second % needed to handle the case where (Index + Num) % NumBitsPerWord = 0
	const uint32 EndMask   = 0xFFFFFFFFu >> ShiftOperand((NumBitsPerWord - (Index + Num) % NumBitsPerWord) % NumBitsPerWord);

	Data += StartIndex;
	if (bValue)
	{
		if (Count == 1)
		{
			*Data |= StartMask & EndMask;
		}
		else
		{
			*Data++ |= StartMask;
			Count -= 2;
			while (Count != 0)
			{
				*Data++ = ~0;
				--Count;
			}
			*Data |= EndMask;
		}
	}
	else
	{
		if (Count == 1)
		{
			*Data &= ~(StartMask & EndMask);
		}
		else
		{
			*Data++ &= ~StartMask;
			Count -= 2;
			while (Count != 0)
			{
				*Data++ = 0;
				--Count;
			}
			*Data &= ~EndMask;
		}
	}

#if VOXEL_DEBUG
	for (int32 It = Index; It < Index + Num; It++)
	{
		check(Get(OriginalData, It) == bValue);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelBitArrayHelpers::CountSetBits(const uint32* RESTRICT Data, const int32 NumWords)
{
	int64 Count = 0;

	{
		const int32 NumWords64 = FVoxelUtilities::DivideFloor_Positive(NumWords, 2);
		const uint64* RESTRICT Data64 = reinterpret_cast<const uint64*>(Data);
		for (int32 WordIndex = 0; WordIndex < NumWords64; WordIndex++)
		{
			Count += FVoxelUtilities::CountBits(Data64[WordIndex]);
		}
	}

	if (NumWords % 2 == 1)
	{
		Count += FVoxelUtilities::CountBits(Data[NumWords - 1]);
	}

	return Count;
}

int64 FVoxelBitArrayHelpers::CountSetBits_UpperBound(const uint32* RESTRICT Data, const int32 NumBits)
{
	int64 Count = 0;

	const int32 NumWords = FVoxelUtilities::DivideFloor_Positive(NumBits, NumBitsPerWord);
	{
		const int32 NumWords64 = FVoxelUtilities::DivideFloor_Positive(NumWords, 2);
		const uint64* RESTRICT Data64 = reinterpret_cast<const uint64*>(Data);
		for (int32 WordIndex = 0; WordIndex < NumWords64; WordIndex++)
		{
			Count += FVoxelUtilities::CountBits(Data64[WordIndex]);
		}
	}

	if (NumWords % 2 == 1)
	{
		Count += FVoxelUtilities::CountBits(Data[NumWords - 1]);
	}

	const int32 NumBitsLeft = NumBits % NumBitsPerWord;
	if (NumBitsLeft > 0)
	{
		Count += FVoxelUtilities::CountBits(Data[NumWords] & ((1u << NumBitsLeft) - 1));
	}

#if VOXEL_DEBUG && 0
		int32 DebugCount = 0;
		for (int32 Index = 0; Index < NumBits; Index++)
		{
			if (Get(Data, Index))
			{
				DebugCount++;
			}
		}
		check(Count == DebugCount);
#endif

	return Count;
}