// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelIterate.h"
#include "VoxelMinimal/VoxelOptional.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

class FVoxelBitReference
{
public:
	FORCEINLINE FVoxelBitReference(uint32& Data, const uint32 Mask)
		: Data(Data)
		, Mask(Mask)
	{
	}

	FORCEINLINE operator bool() const
	{
		return (Data & Mask) != 0;
	}
	FORCEINLINE void operator=(const bool NewValue)
	{
		if (NewValue)
		{
			Data |= Mask;
		}
		else
		{
			Data &= ~Mask;
		}
	}
	FORCEINLINE void operator|=(const bool NewValue)
	{
		if (NewValue)
		{
			Data |= Mask;
		}
	}
	FORCEINLINE void operator&=(const bool NewValue)
	{
		if (!NewValue)
		{
			Data &= ~Mask;
		}
	}
	FORCEINLINE FVoxelBitReference& operator=(const FVoxelBitReference& Copy)
	{
		// As this is emulating a reference, assignment should not rebind,
		// it should write to the referenced bit.
		*this = bool(Copy);
		return *this;
	}

private:
	uint32& Data;
	uint32 Mask;
};

class FVoxelConstBitReference
{
public:
	FORCEINLINE FVoxelConstBitReference(const uint32& Data, const uint32 Mask)
		: Data(Data)
		, Mask(Mask)
	{

	}

	FORCEINLINE operator bool() const
	{
		return (Data & Mask) != 0;
	}

private:
	const uint32& Data;
	uint32 Mask;
};

struct VOXELCORE_API FVoxelBitArrayUtilities
{
public:
	static constexpr int32 NumBitsPerWord = 32;
	static constexpr int32 NumBitsPerWord_Log2 = FVoxelUtilities::ExactLog2<NumBitsPerWord>();
	static constexpr uint32 IndexInWordMask = (1u << NumBitsPerWord_Log2) - 1;
	static constexpr uint32 EmptyWord = 0u;
	static constexpr uint32 FullWord = 0xFFFFFFFFu;

public:
	FORCEINLINE static bool Get(const TConstVoxelArrayView<uint32> Words, const uint32 Index)
	{
		const uint32 Mask = 1u << (Index & IndexInWordMask);
		return Words[Index >> NumBitsPerWord_Log2] & Mask;
	}
	FORCEINLINE static void Set(const TVoxelArrayView<uint32> Words, const uint32 Index, const bool bValue)
	{
		const uint32 Mask = 1u << (Index & IndexInWordMask);
		uint32& Value = Words[Index >> NumBitsPerWord_Log2];

		if (bValue)
		{
			Value |= Mask;
		}
		else
		{
			Value &= ~Mask;
		}
	}

public:
	static bool AllEqual(
		TConstVoxelArrayView<uint32> Words,
		int32 NumBits,
		bool bValue);

	static TVoxelOptional<bool> TryGetAll(
		TConstVoxelArrayView<uint32> Words,
		int32 NumBits);

	static void SetRange(
		TVoxelArrayView<uint32> Data,
		int32 StartIndex,
		int32 Num,
		bool bValue);

private:
	template<typename WordType, typename LambdaType>
	FORCEINLINE static EVoxelIterate ForAllSetBitsInWord(
		WordType Word,
		 const int32 WordIndex,
		 const int32 Num,
		 LambdaType Lambda)
	{
		constexpr int32 NumBitsPerWordInLoop = sizeof(WordType) * 8;

		if (!Word)
		{
			return EVoxelIterate::Continue;
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

					if constexpr (std::is_void_v<LambdaReturnType_T<LambdaType>>)
					{
						Lambda(Index);
					}
					else
					{
						if (Lambda(Index) == EVoxelIterate::Stop)
						{
							return EVoxelIterate::Stop;
						}
					}

					Index++;
				}
				Word >>= NumBitsToCheck;
				continue;
			}

			// Last word should be masked properly
			checkVoxelSlow(Index < Num);

			if constexpr (std::is_void_v<LambdaReturnType_T<LambdaType>>)
			{
				Lambda(Index);
			}
			else
			{
				if (Lambda(Index) == EVoxelIterate::Stop)
				{
					return EVoxelIterate::Stop;
				}
			}

			Word >>= 1;
			Index += 1;
		}
		while (Word);

		return EVoxelIterate::Continue;
	}

public:
	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32)>
	)
	FORCEINLINE static EVoxelIterate ForAllSetBits(
		const TConstVoxelArrayView<uint32> Words,
		const int32 Num,
		LambdaType Lambda)
	{
		const TConstVoxelArrayView<uint64> Words64 =
			Words
			.LeftOf(2 * FVoxelUtilities::DivideFloor_Positive(Words.Num(), 2))
			.ReinterpretAs<uint64>();

		for (const uint64& Word64 : Words64)
		{
			if (FVoxelBitArrayUtilities::ForAllSetBitsInWord(
				Word64,
				&Word64 - Words64.GetData(),
				Num,
				Lambda) == EVoxelIterate::Stop)
			{
				return EVoxelIterate::Stop;
			}
		}

		if (Words.Num() % 2 == 1)
		{
			if (FVoxelBitArrayUtilities::ForAllSetBitsInWord(
				Words[Words.Num() - 1],
				Words.Num() - 1,
				Num,
				Lambda) == EVoxelIterate::Stop)
			{
				return EVoxelIterate::Stop;
			}
		}

		return EVoxelIterate::Continue;
	}
};