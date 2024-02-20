// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelJumpFlood.h"

void FVoxelJumpFlood::JumpFlood2D(
	const FIntPoint& Size,
	const TVoxelArrayView<FIntPoint> InOutClosestPosition)
{
	VOXEL_SCOPE_COUNTER_FORMAT("JumpFlood2D %dx%d", Size.X, Size.Y);
	check(InOutClosestPosition.Num() == Size.X * Size.Y);

	TVoxelArray<FIntPoint> Temp;
	FVoxelUtilities::SetNumFast(Temp, Size.X * Size.Y);

	bool bSourceIsTemp = false;

	const int32 NumPasses = FMath::CeilLogTwo(Size.GetMax());
	for (int32 Pass = 0; Pass < NumPasses; Pass++)
	{
		// -1: we want to start with half the size
		const int32 Step = 1 << (NumPasses - 1 - Pass);

		JumpFlood2DImpl(
			Size,
			bSourceIsTemp ? Temp : InOutClosestPosition,
			bSourceIsTemp ? InOutClosestPosition : Temp,
			Step);

		bSourceIsTemp = !bSourceIsTemp;
	}

	if (bSourceIsTemp)
	{
		VOXEL_SCOPE_COUNTER("Memcpy");
		FVoxelUtilities::Memcpy(InOutClosestPosition, Temp);
	}
}

void FVoxelJumpFlood::JumpFlood2DImpl(
	const FIntPoint& Size,
	const TConstVoxelArrayView<FIntPoint> InData,
	const TVoxelArrayView<FIntPoint> OutData,
	const int32 Step)
{
	VOXEL_FUNCTION_COUNTER();

	checkVoxelSlow(InData.Num() == Size.X * Size.Y);
	checkVoxelSlow(OutData.Num() == Size.X * Size.Y);

	for (int32 Y = 0; Y < Size.Y; Y++)
	{
		int32 Index = Size.X * Y;
		for (int32 X = 0; X < Size.X; X++)
		{
			float BestDistance = MAX_flt;
			FIntPoint BestPosition = MAX_int32;

#define CheckNeighbor(DX, DY) \
			{ \
				const int32 NeighborIndex = Index + DX * Step + DY * Step * Size.X; \
				checkVoxelSlow(NeighborIndex == FVoxelUtilities::Get2DIndex<int32>(Size, X + DX * Step, Y + DY * Step)); \
				const FIntPoint NeighborPosition = InData[NeighborIndex]; \
				const float Distance = FMath::Square<float>(NeighborPosition.X - X) + FMath::Square<float>(NeighborPosition.Y - Y); \
				if (Distance < BestDistance) \
				{ \
					BestDistance = Distance; \
					BestPosition = NeighborPosition; \
				} \
			}

			if (Y - Step >= 0)
			{
				if (X - Step >= 0)
				{
					CheckNeighbor(-1, -1);
				}

				CheckNeighbor(0, -1);

				if (X + Step < Size.X)
				{
					CheckNeighbor(1, -1);
				}
			}

			if (X - Step >= 0)
			{
				CheckNeighbor(-1, 0);
			}

			CheckNeighbor(0, 0);

			if (X + Step < Size.X)
			{
				CheckNeighbor(1, 0);
			}

			if (Y + Step < Size.Y)
			{
				if (X - Step >= 0)
				{
					CheckNeighbor(-1, 1);
				}

				CheckNeighbor(0, 1);

				if (X + Step < Size.X)
				{
					CheckNeighbor(1, 1);
				}
			}

			OutData[Index] = BestPosition;

#undef CheckNeighbor

			Index++;
		}
	}
}