// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

const FVoxelBox2D FVoxelBox2D::Infinite = FVoxelBox2D(FVector2d(-1e50), FVector2d(1e50));
const FVoxelBox2D FVoxelBox2D::InvertedInfinite = []
{
	FVoxelBox2D Box;
	Box.Min = FVector2d(1e50);
	Box.Max = FVector2d(-1e50);
	return Box;
}();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox2D FVoxelBox2D::FromPositions(const TConstVoxelArrayView<FIntPoint> Positions)
{
    VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 128);

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

    return FVoxelBox2D(Min, Max);
}

FVoxelBox2D FVoxelBox2D::FromPositions(const TConstVoxelArrayView<FVector2f> Positions)
{
    VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 128);

    if (Positions.Num() == 0)
    {
        return {};
    }

    FVector2f Min = Positions[0];
    FVector2f Max = Positions[0];

    for (int32 Index = 1; Index < Positions.Num(); Index++)
    {
        Min = FVoxelUtilities::ComponentMin(Min, Positions[Index]);
        Max = FVoxelUtilities::ComponentMax(Max, Positions[Index]);
    }

    return FVoxelBox2D(Min, Max);
}

FVoxelBox2D FVoxelBox2D::FromPositions(const TConstVoxelArrayView<FVector2d> Positions)
{
    VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 128);

    if (Positions.Num() == 0)
    {
        return {};
    }

    FVector2d Min = Positions[0];
    FVector2d Max = Positions[0];

    for (int32 Index = 1; Index < Positions.Num(); Index++)
    {
        Min = FVoxelUtilities::ComponentMin(Min, Positions[Index]);
        Max = FVoxelUtilities::ComponentMax(Max, Positions[Index]);
    }

    return FVoxelBox2D(Min, Max);
}

FVoxelBox2D FVoxelBox2D::FromPositions(
	const TConstVoxelArrayView<float> PositionX,
	const TConstVoxelArrayView<float> PositionY)
{
	const int32 Num = PositionX.Num();
	check(Num == PositionY.Num());
	VOXEL_FUNCTION_COUNTER_NUM(Num, 32);

    if (Num == 0)
    {
        return {};
    }

    const FFloatInterval MinMaxX = FVoxelUtilities::GetMinMax(PositionX);
    const FFloatInterval MinMaxY = FVoxelUtilities::GetMinMax(PositionY);

	return FVoxelBox2D(
        FVector2D(MinMaxX.Min, MinMaxY.Min),
        FVector2D(MinMaxX.Max, MinMaxY.Max));
}

FVoxelBox2D FVoxelBox2D::FromPositions(
	const TConstVoxelArrayView<double> PositionX,
	const TConstVoxelArrayView<double> PositionY)
{
	const int32 Num = PositionX.Num();
	check(Num == PositionY.Num());
	VOXEL_FUNCTION_COUNTER_NUM(Num, 32);

    if (Num == 0)
    {
        return {};
    }

    const FDoubleInterval MinMaxX = FVoxelUtilities::GetMinMax(PositionX);
    const FDoubleInterval MinMaxY = FVoxelUtilities::GetMinMax(PositionY);

	return FVoxelBox2D(
        FVector2D(MinMaxX.Min, MinMaxY.Min),
        FVector2D(MinMaxX.Max, MinMaxY.Max));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelBox2D::ToString() const
{
	return FString::Printf(TEXT("(%f/%f, %f/%f)"), Min.X, Max.X, Min.Y, Max.Y);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFixedArray<FVoxelBox2D, 4> FVoxelBox2D::Difference(const FVoxelBox2D& Other) const
{
    if (!Intersects(Other))
    {
        return { *this };
    }

    TVoxelFixedArray<FVoxelBox2D, 4> OutBoxes;

    if (Min.X < Other.Min.X)
    {
        // Add X min
        OutBoxes.Emplace(FVector2D(Min.X, Min.Y), FVector2D(Other.Min.X, Max.Y));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        OutBoxes.Emplace(FVector2D(Other.Max.X, Min.Y), FVector2D(Max.X, Max.Y));
    }

    const double MinX = FMath::Max(Min.X, Other.Min.X);
    const double MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        OutBoxes.Emplace(FVector2D(MinX, Min.Y), FVector2D(MaxX, Other.Min.Y));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        OutBoxes.Emplace(FVector2D(MinX, Other.Max.Y), FVector2D(MaxX, Max.Y));
    }

    return OutBoxes;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox2D FVoxelBox2D::TransformBy(const FTransform2d& Transform) const
{
	if (IsInfinite())
	{
		return Infinite;
	}

	const FVector2D P00 = Transform.TransformPoint(FVector2D(Min.X, Min.Y));
	const FVector2D P01 = Transform.TransformPoint(FVector2D(Max.X, Min.Y));
	const FVector2D P10 = Transform.TransformPoint(FVector2D(Min.X, Max.Y));
	const FVector2D P11 = Transform.TransformPoint(FVector2D(Max.X, Max.Y));

	FVoxelBox2D NewBox;
	NewBox.Min.X = FMath::Min3(P00.X, P01.X, FMath::Min(P10.X, P11.X));
	NewBox.Min.Y = FMath::Min3(P00.Y, P01.Y, FMath::Min(P10.Y, P11.Y));
	NewBox.Max.X = FMath::Max3(P00.X, P01.X, FMath::Max(P10.X, P11.X));
	NewBox.Max.Y = FMath::Max3(P00.Y, P01.Y, FMath::Max(P10.Y, P11.Y));
	return NewBox;
}