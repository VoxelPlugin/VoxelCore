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

    return FVoxelBox2D(Min, Max);
}

FVoxelBox2D FVoxelBox2D::FromPositions(const TConstVoxelArrayView<FVector2f> Positions)
{
    VOXEL_FUNCTION_COUNTER();

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
    VOXEL_FUNCTION_COUNTER();

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
    VOXEL_FUNCTION_COUNTER();

    const int32 Num = PositionX.Num();
    check(Num == PositionX.Num());
    check(Num == PositionY.Num());

    if (Num == 0)
    {
        return {};
    }

	FVector2f Min = { PositionX[0], PositionY[0] };
	FVector2f Max = { PositionX[0], PositionY[0] };

	for (int32 Index = 1; Index < Num; Index++)
	{
		const FVector2f Position{ PositionX[Index], PositionY[Index] };
		Min = FVoxelUtilities::ComponentMin(Min, Position);
		Max = FVoxelUtilities::ComponentMax(Max, Position);
	}

	return FVoxelBox2D(Min, Max);
}

FVoxelBox2D FVoxelBox2D::FromPositions(
	const TConstVoxelArrayView<double> PositionX,
	const TConstVoxelArrayView<double> PositionY)
{
    VOXEL_FUNCTION_COUNTER();

    const int32 Num = PositionX.Num();
    check(Num == PositionX.Num());
    check(Num == PositionY.Num());

    if (Num == 0)
    {
        return {};
    }

	FVector2D Min = { PositionX[0], PositionY[0] };
	FVector2D Max = { PositionX[0], PositionY[0] };

	for (int32 Index = 1; Index < Num; Index++)
	{
		const FVector2D Position{ PositionX[Index], PositionY[Index] };
		Min = FVoxelUtilities::ComponentMin(Min, Position);
		Max = FVoxelUtilities::ComponentMax(Max, Position);
	}

	return FVoxelBox2D(Min, Max);
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