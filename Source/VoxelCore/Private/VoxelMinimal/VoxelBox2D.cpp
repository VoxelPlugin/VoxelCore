// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"

const FVoxelBox2D FVoxelBox2D::Infinite = FVoxelBox2D(FVector2d(-1e50), FVector2d(1e50));

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