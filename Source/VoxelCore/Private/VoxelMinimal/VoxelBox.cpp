// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"

const FVoxelBox FVoxelBox::Infinite = FVoxelBox(FVector3d(-1e50), FVector3d(1e50));
const FVoxelBox FVoxelBox::InvertedInfinite = []
{
	FVoxelBox Box;
	Box.Min = FVector3d(1e50);
	Box.Max = FVector3d(-1e50);
	return Box;
}();

FVoxelBox FVoxelBox::FromPositions(const TConstVoxelArrayView<FIntVector> Positions)
{
	VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 32);

    if (Positions.Num() == 0)
    {
        return {};
    }

    FIntVector Min = Positions[0];
    FIntVector Max = Positions[0];

    for (int32 Index = 1; Index < Positions.Num(); Index++)
    {
        Min = FVoxelUtilities::ComponentMin(Min, Positions[Index]);
        Max = FVoxelUtilities::ComponentMax(Max, Positions[Index]);
    }

    return FVoxelBox(Min, Max);
}

FVoxelBox FVoxelBox::FromPositions(const TConstVoxelArrayView<FVector3f> Positions)
{
	VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 32);

	if (Positions.Num() == 0)
	{
		return {};
	}
	if (Positions.Num() == 1)
	{
		return FVoxelBox(Positions[0]);
	}

	VectorRegister4Float Min = VectorLoad(&Positions[0].X);
	VectorRegister4Float Max = VectorLoad(&Positions[0].X);

	for (int32 Index = 1; Index < Positions.Num() - 1; Index++)
	{
		const VectorRegister4Float Vector = VectorLoad(&Positions[Index].X);
		Min = VectorMin(Min, Vector);
		Max = VectorMax(Max, Vector);
	}

	FVector3f VectorMin;
	VectorStoreFloat3(Min, &VectorMin);

	FVector3f VectorMax;
	VectorStoreFloat3(Max, &VectorMax);

	VectorMin = FVoxelUtilities::ComponentMin(VectorMin, Positions.Last());
	VectorMax = FVoxelUtilities::ComponentMax(VectorMax, Positions.Last());

	return FVoxelBox(VectorMin, VectorMax);
}

FVoxelBox FVoxelBox::FromPositions(const TConstVoxelArrayView<FVector3d> Positions)
{
    VOXEL_FUNCTION_COUNTER();

    if (Positions.Num() == 0)
    {
        return {};
    }

    FVector3d Min = Positions[0];
    FVector3d Max = Positions[0];

    for (int32 Index = 1; Index < Positions.Num(); Index++)
    {
        Min = FVoxelUtilities::ComponentMin(Min, Positions[Index]);
        Max = FVoxelUtilities::ComponentMax(Max, Positions[Index]);
    }

    return FVoxelBox(Min, Max);
}

FVoxelBox FVoxelBox::FromPositions(
    const TConstVoxelArrayView<float> PositionX,
    const TConstVoxelArrayView<float> PositionY,
    const TConstVoxelArrayView<float> PositionZ)
{
	VOXEL_FUNCTION_COUNTER_NUM(PositionX.Num(), 32);

	if (PositionX.Num() == 0 ||
		PositionY.Num() == 0 ||
		PositionZ.Num() == 0)
	{
		ensure(PositionX.Num() == 0);
		ensure(PositionY.Num() == 0);
		ensure(PositionZ.Num() == 0);
		return {};
	}

	const FFloatInterval MinMaxX = FVoxelUtilities::GetMinMax(PositionX);
	const FFloatInterval MinMaxY = FVoxelUtilities::GetMinMax(PositionY);
	const FFloatInterval MinMaxZ = FVoxelUtilities::GetMinMax(PositionZ);

	FVoxelBox Bounds;

	Bounds.Min.X = MinMaxX.Min;
	Bounds.Min.Y = MinMaxY.Min;
	Bounds.Min.Z = MinMaxZ.Min;

	Bounds.Max.X = MinMaxX.Max;
	Bounds.Max.Y = MinMaxY.Max;
	Bounds.Max.Z = MinMaxZ.Max;

	return Bounds;
}