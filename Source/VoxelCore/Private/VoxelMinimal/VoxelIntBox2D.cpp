﻿// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

// +/- 1024: prevents integers overflow
const FVoxelIntBox2D FVoxelIntBox2D::Infinite = FVoxelIntBox2D(FIntPoint(MIN_int32 + 1024), FIntPoint(MAX_int32 - 1024));
const FVoxelIntBox2D FVoxelIntBox2D::InvertedInfinite = []
{
	FVoxelIntBox2D Box;
	Box.Min = Infinite.Max;
	Box.Max = Infinite.Min;
	return Box;
}();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelIntBox2D FVoxelIntBox2D::FromPositions(const TConstVoxelArrayView<FIntPoint> Positions)
{
	VOXEL_FUNCTION_COUNTER();

	if (Positions.Num() == 0)
	{
		return {};
	}

    FIntPoint Min = Positions[0];
    FIntPoint Max = Positions[0];

    for (int32 Index = 1; Index < Positions.Num(); Index++)
    {
        Min = FVoxelUtilities::ComponentMin(Min, Positions[Index]);
        Max = FVoxelUtilities::ComponentMax(Max, Positions[Index]);
    }

	FVoxelIntBox2D Bounds = FVoxelIntBox2D(Min, Max);

	// Max is exclusive
	Bounds.Max = Bounds.Max + 1;

	return Bounds;
}

FVoxelIntBox2D FVoxelIntBox2D::FromPositions(
	const TConstVoxelArrayView<int32> PositionX,
	const TConstVoxelArrayView<int32> PositionY)
{
	const int32 Num = PositionX.Num();
	check(Num == PositionY.Num());
	VOXEL_FUNCTION_COUNTER_NUM(Num);

	if (Num == 0)
	{
		return {};
	}

	const FInt32Interval MinMaxX = FVoxelUtilities::GetMinMax(PositionX);
	const FInt32Interval MinMaxY = FVoxelUtilities::GetMinMax(PositionY);

	FVoxelIntBox2D Bounds;

	Bounds.Min.X = MinMaxX.Min;
	Bounds.Min.Y = MinMaxY.Min;

	Bounds.Max.X = MinMaxX.Max;
	Bounds.Max.Y = MinMaxY.Max;

	// Max is exclusive
	Bounds.Max = Bounds.Max + 1;

	return Bounds;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelIntBox2D::ToString() const
{
	return FString::Printf(TEXT("(%d/%d, %d/%d)"), Min.X, Max.X, Min.Y, Max.Y);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelIntBox2D FVoxelIntBox2D::Remove_Union(const FVoxelIntBox2D& Other) const
{
    if (!Intersects(Other))
    {
        return *this;
    }

    FVoxelIntBox2D Result = InvertedInfinite;

    if (Min.X < Other.Min.X)
    {
        // Add X min
        Result += FVoxelIntBox2D(FIntPoint(Min.X, Min.Y), FIntPoint(Other.Min.X, Max.Y));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        Result += FVoxelIntBox2D(FIntPoint(Other.Max.X, Min.Y), FIntPoint(Max.X, Max.Y));
    }

    const int32 MinX = FMath::Max(Min.X, Other.Min.X);
    const int32 MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        Result += FVoxelIntBox2D(FIntPoint(MinX, Min.Y), FIntPoint(MaxX, Other.Min.Y));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        Result += FVoxelIntBox2D(FIntPoint(MinX, Other.Max.Y), FIntPoint(MaxX, Max.Y));
    }

	if (!Result.IsValid())
	{
		return {};
	}

    return Result;
}

void FVoxelIntBox2D::Remove_Split(
    const FVoxelIntBox2D& Other,
    TVoxelArray<FVoxelIntBox2D>& OutRemainder) const
{
    if (!Intersects(Other))
    {
        OutRemainder.Add(*this);
        return;
    }

    if (Min.X < Other.Min.X)
    {
        // Add X min
        OutRemainder.Emplace(FIntPoint(Min.X, Min.Y), FIntPoint(Other.Min.X, Max.Y));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        OutRemainder.Emplace(FIntPoint(Other.Max.X, Min.Y), FIntPoint(Max.X, Max.Y));
    }

    const int32 MinX = FMath::Max(Min.X, Other.Min.X);
    const int32 MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        OutRemainder.Emplace(FIntPoint(MinX, Min.Y), FIntPoint(MaxX, Other.Min.Y));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        OutRemainder.Emplace(FIntPoint(MinX, Other.Max.Y), FIntPoint(MaxX, Max.Y));
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelIntBox2D::Subdivide(
	const int32 ChildrenSize,
	TVoxelArray<FVoxelIntBox2D>& OutChildren,
	const bool bUseOverlap,
	const int32 MaxChildren) const
{
	OutChildren.Reset();

	const FIntPoint LowerBound = FVoxelUtilities::DivideFloor(Min, ChildrenSize) * ChildrenSize;
	const FIntPoint UpperBound = FVoxelUtilities::DivideCeil(Max, ChildrenSize) * ChildrenSize;

	const FIntPoint EstimatedSize = (UpperBound - LowerBound) / ChildrenSize;

	VOXEL_FUNCTION_COUNTER_NUM(EstimatedSize.X * EstimatedSize.Y, 128);

	OutChildren.Reserve(EstimatedSize.X * EstimatedSize.Y);

	for (int32 X = LowerBound.X; X < UpperBound.X; X += ChildrenSize)
	{
		for (int32 Y = LowerBound.Y; Y < UpperBound.Y; Y += ChildrenSize)
		{
			FVoxelIntBox2D Child(FIntPoint(X, Y), FIntPoint(X + ChildrenSize, Y + ChildrenSize));
			if (bUseOverlap)
			{
				Child = Child.IntersectWith(*this);
			}
			OutChildren.Add(Child);

			if (MaxChildren != -1 &&
				OutChildren.Num() > MaxChildren)
			{
				return false;
			}
		}
	}
	return true;
}