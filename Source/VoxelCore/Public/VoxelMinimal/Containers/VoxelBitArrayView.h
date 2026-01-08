// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelIntBox2D.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

struct FVoxelSetBitIterator;

class VOXELCORE_API FVoxelBitReference
{
public:
	FORCEINLINE FVoxelBitReference(
		uint64& Word,
		const uint64 Mask)
		: Word(Word)
		, Mask(Mask)
	{
	}
	UE_NONCOPYABLE(FVoxelBitReference);

	FORCEINLINE operator bool() const
	{
		return (Word & Mask) != 0;
	}
	FORCEINLINE void operator=(const bool NewValue)
	{
		if (NewValue)
		{
			Word |= Mask;
		}
		else
		{
			Word &= ~Mask;
		}
	}
	FORCEINLINE void operator|=(const bool NewValue)
	{
		if (NewValue)
		{
			Word |= Mask;
		}
	}
	FORCEINLINE void operator&=(const bool NewValue)
	{
		if (!NewValue)
		{
			Word &= ~Mask;
		}
	}

private:
	uint64& Word;
	uint64 Mask;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelConstBitReference
{
public:
	FORCEINLINE FVoxelConstBitReference(
		const uint64& Word,
		const uint64 Mask)
		: Word(Word)
		, Mask(Mask)
	{
	}
	UE_NONCOPYABLE(FVoxelConstBitReference);

	FORCEINLINE operator bool() const
	{
		return (Word & Mask) != 0;
	}

private:
	const uint64& Word;
	uint64 Mask;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FConstVoxelBitArrayView
{
public:
	static constexpr int32 NumBitsPerWord = 64;
	static constexpr int32 NumBitsPerWord_Log2 = FVoxelUtilities::ExactLog2<NumBitsPerWord>();
	static constexpr uint64 WordMask = (1_u64 << NumBitsPerWord_Log2) - 1;
	static constexpr uint64 EmptyWord = 0_u64;
	static constexpr uint64 FullWord = 0xFFFFFFFFFFFFFFFF_u64;

public:
	FConstVoxelBitArrayView() = default;

	FORCEINLINE FConstVoxelBitArrayView(
		const uint64* Allocation,
		const int32 NumBits)
		: Allocation(Allocation)
		, NumBits(NumBits)
	{
	}

public:
	FORCEINLINE int32 Num() const
	{
		return NumBits;
	}
	FORCEINLINE int32 NumWords() const
	{
		return FVoxelUtilities::DivideCeil_Positive_Log2(NumBits, NumBitsPerWord_Log2);
	}

public:
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < NumBits;
	}

	FORCEINLINE FVoxelConstBitReference operator[](const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));

		return FVoxelConstBitReference(
			GetWord(Index >> NumBitsPerWord_Log2),
			1_u64 << (Index & WordMask));
	}

public:
	FORCEINLINE const uint64* GetWordData() const
	{
		return Allocation;
	}
	FORCEINLINE TConstVoxelArrayView<uint64> GetWordView() const
	{
		return { GetWordData(), NumWords() };
	}
	FORCEINLINE const uint64& GetWord(const int32 Index) const
	{
		return GetWordView()[Index];
	}

public:
	FORCEINLINE bool operator==(const FConstVoxelBitArrayView& Other) const
	{
		if (Num() != Other.Num())
		{
			return false;
		}

		return FVoxelUtilities::Equal(GetWordView(), Other.GetWordView());
	}

	FVoxelSetBitIterator IterateSetBits() const;

public:
	FORCEINLINE FConstVoxelBitArrayView LeftOf(const int32 Count) const
	{
		checkVoxelSlow(Count <= Num());
		return FConstVoxelBitArrayView(GetWordData(), Count);
	}

public:
	TVoxelOptional<bool> TryGetAll() const;
	bool AllEqual(bool bValue) const;
	int32 CountSetBits() const;

public:
	FORCEINLINE bool TestRange(
		const int32 StartIndex,
		const int32 NumToClear) const
	{
		checkVoxelSlow(NumToClear > 0);
		checkVoxelSlow(0 <= StartIndex && StartIndex + NumToClear <= Num());

		const int32 EndIndex = StartIndex + NumToClear;

		const int32 FirstWordIndex = FVoxelUtilities::DivideFloor_Positive(StartIndex, NumBitsPerWord);
		const int32 LastWordIndex = FVoxelUtilities::DivideFloor_Positive(EndIndex - 1, NumBitsPerWord);

		const int32 NumWords = 1 + LastWordIndex - FirstWordIndex;
		checkVoxelSlow(NumWords >= 1);

		const uint64 StartMask = FullWord << (StartIndex & WordMask);
		const uint64 EndMask = FullWord >> ((-EndIndex) & WordMask);

		const TConstVoxelArrayView<uint64> Words = GetWordView();

		if (NumWords == 1)
		{
			const uint64 Mask = StartMask & EndMask;
			const uint64 Word = Words[FirstWordIndex];

			return (Word & Mask) == Mask;
		}

		if ((Words[FirstWordIndex] & StartMask) != StartMask)
		{
			return false;
		}

		for (int32 Index = FirstWordIndex + 1; Index < LastWordIndex; Index++)
		{
			if (Words[Index] != FullWord)
			{
				return false;
			}
		}

		if ((Words[LastWordIndex] & EndMask) != EndMask)
		{
			return false;
		}

		return true;
	}

protected:
	const uint64* Allocation = nullptr;
	int32 NumBits = 0;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelBitArrayView : public FConstVoxelBitArrayView
{
public:
	FVoxelBitArrayView() = default;

	FORCEINLINE FVoxelBitArrayView(
		uint64* Allocation,
		const int32 NumBits)
		: FConstVoxelBitArrayView(Allocation, NumBits)
	{
	}

public:
	FORCEINLINE uint64* GetWordData() const
	{
		return ConstCast(Allocation);
	}
	FORCEINLINE TVoxelArrayView<uint64> GetWordView() const
	{
		return { GetWordData(), NumWords() };
	}
	FORCEINLINE uint64& GetWord(const int32 Index) const
	{
		return GetWordView()[Index];
	}

public:
	FORCEINLINE FVoxelBitArrayView LeftOf(const int32 Count) const
	{
		checkVoxelSlow(Count <= Num());
		return FVoxelBitArrayView(GetWordData(), Count);
	}

public:
	FORCEINLINE FVoxelBitReference operator[](const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));

		return FVoxelBitReference(
			GetWord(Index >> NumBitsPerWord_Log2),
			1_u64 << (Index & WordMask));
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue) const
	{
		uint64& Word = GetWord(Index >> NumBitsPerWord_Log2);
		const uint64 Mask = 1_u64 << (Index & WordMask);

		if (bValue)
		{
			return ReinterpretCastRef<TVoxelAtomic<uint64>>(Word).Or_ReturnOld(Mask) & Mask;
		}
		else
		{
			return ReinterpretCastRef<TVoxelAtomic<uint64>>(Word).And_ReturnOld(~Mask) & Mask;
		}
	}
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue) const
	{
		(void)AtomicSet_ReturnOld(Index, bValue);
	}

public:
	void BitwiseOr(const FConstVoxelBitArrayView& Other) const;
	void BitwiseAnd(const FConstVoxelBitArrayView& Other) const;

	// Values will be zeroed
	TVoxelArray<FVoxelIntBox2D> GreedyMeshing2D(const FIntPoint& Size) const;
	TVoxelArray<FVoxelIntBox> GreedyMeshing3D(const FIntVector& Size) const;

public:
	FORCEINLINE void SetRange(
		const int32 StartIndex,
		const int32 NumToSet,
		const bool bValue) const
	{
		checkVoxelSlow(0 <= StartIndex && StartIndex + NumToSet <= Num());

		if (NumToSet == 0)
		{
			return;
		}

		const int32 EndIndex = StartIndex + NumToSet;

		const int32 FirstWordIndex = FVoxelUtilities::DivideFloor_Positive(StartIndex, NumBitsPerWord);
		const int32 LastWordIndex = FVoxelUtilities::DivideFloor_Positive(EndIndex - 1, NumBitsPerWord);

		const int32 NumWords = 1 + LastWordIndex - FirstWordIndex;
		checkVoxelSlow(NumWords >= 1);

		const uint64 StartMask = FullWord << (StartIndex & WordMask);
		const uint64 EndMask = FullWord >> ((-EndIndex) & WordMask);

		const TVoxelArrayView<uint64> Words = GetWordView();

		if (bValue)
		{
			if (NumWords == 1)
			{
				Words[FirstWordIndex] |= StartMask & EndMask;
			}
			else
			{
				Words[FirstWordIndex] |= StartMask;

				for (int32 Index = FirstWordIndex + 1; Index < LastWordIndex; Index++)
				{
					Words[Index] = FullWord;
				}

				Words[LastWordIndex] |= EndMask;
			}
		}
		else
		{
			if (NumWords == 1)
			{
				Words[FirstWordIndex] &= ~(StartMask & EndMask);
			}
			else
			{
				Words[FirstWordIndex] &= ~StartMask;

				for (int32 Index = FirstWordIndex + 1; Index < LastWordIndex; Index++)
				{
					Words[Index] = EmptyWord;
				}

				Words[LastWordIndex] &= ~EndMask;
			}
		}
	}

public:
	FORCEINLINE bool TestAndClear(const int32 Index) const
	{
		uint64& Word = GetWord(Index >> NumBitsPerWord_Log2);
		const uint64 Mask = 1_u64 << (Index & WordMask);

		if ((Word & Mask) == 0)
		{
			return false;
		}

		Word &= ~Mask;
		return true;
	}
	FORCEINLINE bool TestAndClearRange(
		const int32 StartIndex,
		const int32 NumToClear) const
	{
		checkVoxelSlow(NumToClear > 0);
		checkVoxelSlow(0 <= StartIndex && StartIndex + NumToClear <= Num());

		const int32 EndIndex = StartIndex + NumToClear;

		const int32 FirstWordIndex = FVoxelUtilities::DivideFloor_Positive(StartIndex, NumBitsPerWord);
		const int32 LastWordIndex = FVoxelUtilities::DivideFloor_Positive(EndIndex - 1, NumBitsPerWord);

		const int32 NumWords = 1 + LastWordIndex - FirstWordIndex;
		checkVoxelSlow(NumWords >= 1);

		const uint64 StartMask = FullWord << (StartIndex & WordMask);
		const uint64 EndMask = FullWord >> ((-EndIndex) & WordMask);

		const TVoxelArrayView<uint64> Words = GetWordView();

		if (NumWords == 1)
		{
			const uint64 Mask = StartMask & EndMask;
			uint64& Word = Words[FirstWordIndex];

			if ((Word & Mask) != Mask)
			{
				return false;
			}

			Word &= ~Mask;
			return true;
		}

		// Test
		{
			if ((Words[FirstWordIndex] & StartMask) != StartMask)
			{
				return false;
			}

			for (int32 Index = FirstWordIndex + 1; Index < LastWordIndex; Index++)
			{
				if (Words[Index] != FullWord)
				{
					return false;
				}
			}

			if ((Words[LastWordIndex] & EndMask) != EndMask)
			{
				return false;
			}
		}

		// Clear
		{
			Words[FirstWordIndex] &= ~StartMask;

			for (int32 Index = FirstWordIndex + 1; Index < LastWordIndex; Index++)
			{
				Words[Index] = EmptyWord;
			}

			Words[LastWordIndex] &= ~EndMask;
		}

		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelSetBitIterator : TVoxelIterator<FVoxelSetBitIterator>
{
public:
	FVoxelSetBitIterator() = default;
	FORCEINLINE explicit FVoxelSetBitIterator(const FConstVoxelBitArrayView View)
		: NumBits(View.Num())
		, WordPtr(View.GetWordData())
	{
		if (!WordPtr)
		{
			return;
		}

		Word = *WordPtr;
		GoToNextSetBit();
	}

	FORCEINLINE int32 GetIndex() const
	{
		return Index;
	}

	FORCEINLINE int32 operator*() const
	{
		return GetIndex();
	}
	FORCEINLINE void operator++()
	{
		Index++;
		Word >>= 1;
		BitsLeftInWord--;

		GoToNextSetBit();
	}
	FORCEINLINE operator bool() const
	{
		return Index < NumBits;
	}

private:
	int32 Index = 0;
	int32 NumBits = 0;
	const uint64* WordPtr = nullptr;
	uint64 Word = 0;
	int32 BitsLeftInWord = FConstVoxelBitArrayView::NumBitsPerWord;

	FORCEINLINE void GoToNextSetBit()
	{
		ON_SCOPE_EXIT
		{
			if (Index < NumBits)
			{
				checkVoxelSlow(Word & 0x1);
				checkVoxelSlow(BitsLeftInWord > 0);
			}
		};

		checkVoxelSlow(BitsLeftInWord >= 0);
		checkVoxelSlow(BitsLeftInWord > 0 || !Word);

		if (Word & 0x1)
		{
			// Fast path
			return;
		}

		if (Word)
		{
			// There's still a valid index, skip to that

			const uint64 FirstBitIndex = FVoxelUtilities::FirstBitLow(Word);
			checkVoxelSlow(Word & (1_u64 << FirstBitIndex));

			Index += FirstBitIndex;
			Word >>= FirstBitIndex;
			BitsLeftInWord -= FirstBitIndex;

			return;
		}

		// Skip forward
		WordPtr++;
		Index += BitsLeftInWord;

		if (Index >= NumBits)
		{
			return;
		}

		Word = *WordPtr;
		BitsLeftInWord = FConstVoxelBitArrayView::NumBitsPerWord;

		while (!Word)
		{
			// Skip forward
			WordPtr++;
			Index += FConstVoxelBitArrayView::NumBitsPerWord;

			if (Index >= NumBits)
			{
				return;
			}

			Word = *WordPtr;
		}

		if (Word & 0x1)
		{
			// Fast path
			return;
		}

		const uint64 FirstBitIndex = FVoxelUtilities::FirstBitLow(Word);
		checkVoxelSlow(Word & (1_u64 << FirstBitIndex));

		Index += FirstBitIndex;
		Word >>= FirstBitIndex;
		BitsLeftInWord -= FirstBitIndex;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FVoxelSetBitIterator FConstVoxelBitArrayView::IterateSetBits() const
{
	return FVoxelSetBitIterator(*this);
}