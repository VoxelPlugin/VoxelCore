// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

// +/- 1024: prevents integers overflow
const FVoxelIntBox FVoxelIntBox::Infinite = FVoxelIntBox(FIntVector(MIN_int32 + 1024), FIntVector(MAX_int32 - 1024));
const FVoxelIntBox FVoxelIntBox::InvertedInfinite = []
{
	FVoxelIntBox Box;
	Box.Min = Infinite.Max;
	Box.Max = Infinite.Min;
	return Box;
}();

FVoxelIntBox FVoxelIntBox::FromPositions(const TVoxelArrayView<const FIntVector> Positions)
{
    VOXEL_FUNCTION_COUNTER();

    if (Positions.Num() == 0)
    {
        return {};
    }
    if (Positions.Num() == 1)
    {
        return FVoxelIntBox(Positions[0]);
    }

    VectorRegister4Int BoundsMin = VectorIntLoad(&Positions[0]);
    VectorRegister4Int BoundsMax = BoundsMin;

    for (int32 Index = 1; Index < Positions.Num() - 1; Index++)
    {
        VectorRegister4Int Position = VectorIntLoad(&Positions[Index]);

        BoundsMin = VectorIntMin(BoundsMin, Position);
        BoundsMax = VectorIntMax(BoundsMax, Position);
    }

	FIntVector4 PaddedMin;
	FIntVector4 PaddedMax;
	VectorIntStore(BoundsMin, &PaddedMin);
	VectorIntStore(BoundsMax, &PaddedMax);

    FVoxelIntBox Bounds;
    Bounds.Min = FIntVector(PaddedMin.X, PaddedMin.Y, PaddedMin.Z);
    Bounds.Max = FIntVector(PaddedMax.X, PaddedMax.Y, PaddedMax.Z);

    // Last doesn't have padding at the end to do a VectorIntLoad, so do it manually
    Bounds.Min = FVoxelUtilities::ComponentMin(Bounds.Min, Positions.Last());
    Bounds.Max = FVoxelUtilities::ComponentMax(Bounds.Max, Positions.Last());

    // Max is exclusive
    Bounds.Max = Bounds.Max + 1;

    return Bounds;
}

TArray<FVoxelIntBox, TFixedAllocator<6>> FVoxelIntBox::Difference(const FVoxelIntBox& Other) const
{
    if (!Intersect(Other))
    {
        return { *this };
    }

    TArray<FVoxelIntBox, TFixedAllocator<6>> OutBoxes;

    if (Min.Z < Other.Min.Z)
    {
        // Add bottom
        OutBoxes.Emplace(Min, FIntVector(Max.X, Max.Y, Other.Min.Z));
    }
    if (Other.Max.Z < Max.Z)
    {
        // Add top
        OutBoxes.Emplace(FIntVector(Min.X, Min.Y, Other.Max.Z), Max);
    }

    const int32 MinZ = FMath::Max(Min.Z, Other.Min.Z);
    const int32 MaxZ = FMath::Min(Max.Z, Other.Max.Z);

    if (Min.X < Other.Min.X)
    {
        // Add X min
        OutBoxes.Emplace(FIntVector(Min.X, Min.Y, MinZ), FIntVector(Other.Min.X, Max.Y, MaxZ));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        OutBoxes.Emplace(FIntVector(Other.Max.X, Min.Y, MinZ), FIntVector(Max.X, Max.Y, MaxZ));
    }

    const int32 MinX = FMath::Max(Min.X, Other.Min.X);
    const int32 MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        OutBoxes.Emplace(FIntVector(MinX, Min.Y, MinZ), FIntVector(MaxX, Other.Min.Y, MaxZ));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        OutBoxes.Emplace(FIntVector(MinX, Other.Max.Y, MinZ), FIntVector(MaxX, Max.Y, MaxZ));
    }

    return OutBoxes;
}
