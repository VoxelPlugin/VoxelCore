// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

TVoxelOptional<bool> FConstVoxelBitArrayView::TryGetAll() const
{
	if (Num() == 0)
	{
		return {};
	}

	const bool bValue = (*this)[0];

	if (!AllEqual(bValue))
	{
		return {};
	}

	return bValue;
}

bool FConstVoxelBitArrayView::AllEqual(const bool bValue) const
{
	VOXEL_FUNCTION_COUNTER_NUM(Num(), 4096);

	const int32 NumFullWords = FVoxelUtilities::DivideFloor_Positive(NumBits, NumBitsPerWord);

	if (!FVoxelUtilities::AllEqual(GetWordView().LeftOf(NumFullWords), bValue ? FullWord : EmptyWord))
	{
		return false;
	}

	const int32 NumBitsInLastWord = NumBits & WordMask;
	checkVoxelSlow(NumBitsInLastWord == NumBits - NumFullWords * NumBitsPerWord);

	if (NumBitsInLastWord == 0)
	{
		return true;
	}

	const uint64 LastWord = GetWord(NumFullWords);
	const uint64 Mask = (1_u64 << NumBitsInLastWord) - 1;

	if (bValue)
	{
		return (LastWord & Mask) == Mask;
	}
	else
	{
		return (LastWord & Mask) == 0;
	}
}

int32 FConstVoxelBitArrayView::CountSetBits() const
{
	VOXEL_FUNCTION_COUNTER_NUM(Num(), 4096);

	const int32 NumFullWords = FVoxelUtilities::DivideFloor_Positive(NumBits, NumBitsPerWord);

	int32 Count = 0;
	for (const uint64 Word : GetWordView().LeftOf(NumFullWords))
	{
		Count += FVoxelUtilities::CountBits(Word);
	}

	const int32 NumBitsInLastWord = NumBits & WordMask;
	checkVoxelSlow(NumBitsInLastWord == NumBits - NumFullWords * NumBitsPerWord);

	if (NumBitsInLastWord != 0)
	{
		Count += FVoxelUtilities::CountBits(GetWord(NumFullWords) & ((1_u64 << NumBitsInLastWord) - 1));
	}

	return Count;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBitArrayView::BitwiseOr(const FConstVoxelBitArrayView& Other) const
{
	VOXEL_FUNCTION_COUNTER_NUM(this->Num(), 128);
	checkVoxelSlow(this->Num() == Other.Num());

	for (int32 WordIndex = 0; WordIndex < this->NumWords(); WordIndex++)
	{
		GetWord(WordIndex) |= Other.GetWord(WordIndex);
	}
}

void FVoxelBitArrayView::BitwiseAnd(const FConstVoxelBitArrayView& Other) const
{
	VOXEL_FUNCTION_COUNTER_NUM(this->Num(), 128);
	checkVoxelSlow(this->Num() == Other.Num());

	for (int32 WordIndex = 0; WordIndex < this->NumWords(); WordIndex++)
	{
		GetWord(WordIndex) &= Other.GetWord(WordIndex);
	}
}

TVoxelArray<FVoxelIntBox2D> FVoxelBitArrayView::GreedyMeshing2D(const FIntPoint& Size) const
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y);
	checkVoxelSlow(Num() == Size.X * Size.Y);

	const auto TestAndClearPoint = [&](const int32 X, const int32 Y)
	{
		checkVoxelSlow(0 <= X && X <= Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);

		return TestAndClear(X + Size.X * Y);
	};
	const auto TestAndClearLine = [&](const int32 X, const int32 Width, const int32 Y)
	{
		checkVoxelSlow(0 <= X && X + Width <= Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);

		return TestAndClearRange(X + Size.X * Y, Width);
	};

	TVoxelArray<FVoxelIntBox2D> Result;
	Result.Reserve(Size.GetMax());

	for (int32 X = 0; X < Size.X; X++)
	{
		for (int32 Y = 0; Y < Size.Y;)
		{
			if (!TestAndClearPoint(X, Y))
			{
				Y++;
				continue;
			}

			int32 Width = 1;
			while (X + Width < Size.X && TestAndClearPoint(X + Width, Y))
			{
				Width++;
			}

			int32 Height = 1;
			while (Y + Height < Size.Y && TestAndClearLine(X, Width, Y + Height))
			{
				Height++;
			}

			Result.Add(FVoxelIntBox2D(
				FIntPoint(X, Y),
				FIntPoint(X + Width, Y + Height)));

			Y += Height;
		}
	}

	checkVoxelSlow(AllEqual(false));
	return Result;
}