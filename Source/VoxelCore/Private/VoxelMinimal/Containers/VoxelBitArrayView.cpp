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
		checkVoxelSlow(0 <= X && X < Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);

		return TestAndClear(X + Size.X * Y);
	};
	const auto TestAndClearLine = [&](const int32 X, const int32 SizeX, const int32 Y)
	{
		checkVoxelSlow(0 <= X && X + SizeX <= Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);

		return TestAndClearRange(X + Size.X * Y, SizeX);
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

			int32 SizeX = 1;
			while (X + SizeX < Size.X && TestAndClearPoint(X + SizeX, Y))
			{
				SizeX++;
			}

			int32 SizeY = 1;
			while (Y + SizeY < Size.Y && TestAndClearLine(X, SizeX, Y + SizeY))
			{
				SizeY++;
			}

			Result.Add(FVoxelIntBox2D(
				FIntPoint(X, Y),
				FIntPoint(X + SizeX, Y + SizeY)));

			Y += SizeY;
		}
	}

	checkVoxelSlow(AllEqual(false));
	return Result;
}

TVoxelArray<FVoxelIntBox> FVoxelBitArrayView::GreedyMeshing3D(const FIntVector& Size) const
{
	VOXEL_FUNCTION_COUNTER_NUM(Size.X * Size.Y * Size.Z);
	checkVoxelSlow(Num() == Size.X * Size.Y * Size.Z);

	TVoxelArray<FVoxelIntBox> Result;
	Result.Reserve(Size.GetMax());

	const auto TestAndClearPoint = [&](
		const int32 X,
		const int32 Y,
		const int32 Z)
	{
		checkVoxelSlow(0 <= X && X < Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);

		return TestAndClear(X + Size.X * Y + Size.X * Size.Y * Z);
	};
	const auto TestAndClearLine = [&](
		const int32 X,
		const int32 SizeX,
		const int32 Y,
		const int32 Z)
	{
		checkVoxelSlow(0 <= X && X + SizeX <= Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);

		return TestAndClearRange(
			X + Size.X * Y + Size.X * Size.Y * Z,
			SizeX);
	};
	const auto TestAndClearBlock = [&](
		const int32 X,
		const int32 SizeX,
		const int32 Y,
		const int32 SizeY,
		const int32 Z)
	{
		checkVoxelSlow(0 <= X && X + SizeX <= Size.X);
		checkVoxelSlow(0 <= Y && Y + SizeY <= Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);

		for (int32 Index = 0; Index < SizeY; Index++)
		{
			if (!TestRange(
				FVoxelUtilities::Get3DIndex<int32>(Size, X, Y + Index, Z),
				SizeX))
			{
				return false;
			}
		}

		for (int32 Index = 0; Index < SizeY; Index++)
		{
			SetRange(
				FVoxelUtilities::Get3DIndex<int32>(Size, X, Y + Index, Z),
				SizeX,
				false);
		}
		return true;
	};

	for (int32 X = 0; X < Size.X; X++)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 Z = 0; Z < Size.Z;)
			{
				if (!(*this)[FVoxelUtilities::Get3DIndex<int32>(Size, X, Y, Z)])
				{
					Z++;
					continue;
				}

				int32 SizeX = 1;
				while (
					X + SizeX < Size.X &&
					TestAndClearPoint(X + SizeX, Y, Z))
				{
					SizeX++;
				}

				int32 SizeY = 1;
				while (
					Y + SizeY < Size.Y &&
					TestAndClearLine(X, SizeX, Y + SizeY, Z))
				{
					SizeY++;
				}

				int32 SizeZ = 1;
				while (
					Z + SizeZ < Size.Z &&
					TestAndClearBlock(X, SizeX, Y, SizeY, Z + SizeZ))
				{
					SizeZ++;
				}

				Result.Add(FVoxelIntBox(
					FIntVector(X, Y, Z),
					FIntVector(X + SizeX, Y + SizeY, Z + SizeZ)));

				Z += SizeZ;
			}
		}
	}

	return Result;
}