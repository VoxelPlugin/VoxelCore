// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

const FVoxelBox FVoxelBox::Infinite = FVoxelBox(FVector3d(-1e30), FVector3d(1e30));
const FVoxelBox FVoxelBox::InvertedInfinite = []
{
	FVoxelBox Box;
	Box.Min = FVector3d(1e30);
	Box.Max = FVector3d(-1e30);
	return Box;
}();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox FVoxelBox::FromBounds(const TConstVoxelArrayView<FVoxelBox> Bounds)
{
	VOXEL_FUNCTION_COUNTER_NUM(Bounds.Num(), 32);

    if (Bounds.Num() == 0)
    {
        return {};
    }

	FVoxelBox Result = Bounds[0];
    for (int32 Index = 1; Index < Bounds.Num(); Index++)
    {
		Result += Bounds[Index];
    }
	return Result;
}

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

FVoxelBox FVoxelBox::FromPositions(const TConstVoxelArrayView<FVector4f> Positions)
{
	VOXEL_FUNCTION_COUNTER_NUM(Positions.Num(), 32);
	checkStatic(sizeof(FVector4f) == sizeof(VectorRegister4Float));

	if (Positions.Num() == 0)
	{
		return {};
	}
	if (Positions.Num() == 1)
	{
		return FVoxelBox(Positions[0]);
	}

	const TConstVoxelArrayView<VectorRegister4Float> VectorPositions = Positions.ReinterpretAs<VectorRegister4Float>();

	VectorRegister4Float Min = VectorPositions[0];
	VectorRegister4Float Max = VectorPositions[0];

	for (const VectorRegister4Float& Position : VectorPositions)
	{
		Min = VectorMin(Min, Position);
		Max = VectorMax(Max, Position);
	}

	return FVoxelBox(
		FVector3f(ReinterpretCastRef<FVector4f>(Min)),
		FVector3f(ReinterpretCastRef<FVector4f>(Max)));
}

FVoxelBox FVoxelBox::FromPositions(
    const TConstVoxelArrayView<float> PositionX,
    const TConstVoxelArrayView<float> PositionY,
    const TConstVoxelArrayView<float> PositionZ)
{
	const int32 Num = PositionX.Num();
	check(Num == PositionY.Num());
	check(Num == PositionZ.Num());
	VOXEL_FUNCTION_COUNTER_NUM(Num, 32);

	if (Num == 0)
	{
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

FVoxelBox FVoxelBox::FromPositions(
    const TConstVoxelArrayView<double> PositionX,
    const TConstVoxelArrayView<double> PositionY,
    const TConstVoxelArrayView<double> PositionZ)
{
	const int32 Num = PositionX.Num();
	check(Num == PositionY.Num());
	check(Num == PositionZ.Num());
	VOXEL_FUNCTION_COUNTER_NUM(Num, 32);

	if (Num == 0)
	{
		return {};
	}

	const FDoubleInterval MinMaxX = FVoxelUtilities::GetMinMax(PositionX);
	const FDoubleInterval MinMaxY = FVoxelUtilities::GetMinMax(PositionY);
	const FDoubleInterval MinMaxZ = FVoxelUtilities::GetMinMax(PositionZ);

	FVoxelBox Bounds;

	Bounds.Min.X = MinMaxX.Min;
	Bounds.Min.Y = MinMaxY.Min;
	Bounds.Min.Z = MinMaxZ.Min;

	Bounds.Max.X = MinMaxX.Max;
	Bounds.Max.Y = MinMaxY.Max;
	Bounds.Max.Z = MinMaxZ.Max;

	return Bounds;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelBox::ToString() const
{
	return FString::Printf(TEXT("(%f/%f, %f/%f, %f/%f)"), Min.X, Max.X, Min.Y, Max.Y, Min.Z, Max.Z);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox FVoxelBox::Remove_Union(const FVoxelBox& Other) const
{
    if (!Intersects(Other))
    {
        return *this;
    }

	FVoxelBox Result = InvertedInfinite;

    if (Min.Z < Other.Min.Z)
    {
        // Add bottom
        Result += FVoxelBox(Min, FVector(Max.X, Max.Y, Other.Min.Z));
    }
    if (Other.Max.Z < Max.Z)
    {
        // Add top
        Result += FVoxelBox(FVector(Min.X, Min.Y, Other.Max.Z), Max);
    }

    const double MinZ = FMath::Max(Min.Z, Other.Min.Z);
    const double MaxZ = FMath::Min(Max.Z, Other.Max.Z);

    if (Min.X < Other.Min.X)
    {
        // Add X min
        Result += FVoxelBox(FVector(Min.X, Min.Y, MinZ), FVector(Other.Min.X, Max.Y, MaxZ));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        Result += FVoxelBox(FVector(Other.Max.X, Min.Y, MinZ), FVector(Max.X, Max.Y, MaxZ));
    }

    const double MinX = FMath::Max(Min.X, Other.Min.X);
    const double MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        Result += FVoxelBox(FVector(MinX, Min.Y, MinZ), FVector(MaxX, Other.Min.Y, MaxZ));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        Result += FVoxelBox(FVector(MinX, Other.Max.Y, MinZ), FVector(MaxX, Max.Y, MaxZ));
    }

	if (!Result.IsValid())
	{
		return {};
	}

    return Result;
}

void FVoxelBox::Remove_Split(
	const FVoxelBox& Other,
	TVoxelArray<FVoxelBox>& OutRemainder) const
{
    if (!Intersects(Other))
    {
        OutRemainder.Add(*this);
        return;
    }

    if (Min.Z < Other.Min.Z)
    {
        // Add bottom
        OutRemainder.Emplace(Min, FVector(Max.X, Max.Y, Other.Min.Z));
    }
    if (Other.Max.Z < Max.Z)
    {
        // Add top
        OutRemainder.Emplace(FVector(Min.X, Min.Y, Other.Max.Z), Max);
    }

    const double MinZ = FMath::Max(Min.Z, Other.Min.Z);
    const double MaxZ = FMath::Min(Max.Z, Other.Max.Z);

    if (Min.X < Other.Min.X)
    {
        // Add X min
        OutRemainder.Emplace(FVector(Min.X, Min.Y, MinZ), FVector(Other.Min.X, Max.Y, MaxZ));
    }
    if (Other.Max.X < Max.X)
    {
        // Add X max
        OutRemainder.Emplace(FVector(Other.Max.X, Min.Y, MinZ), FVector(Max.X, Max.Y, MaxZ));
    }

    const double MinX = FMath::Max(Min.X, Other.Min.X);
    const double MaxX = FMath::Min(Max.X, Other.Max.X);

    if (Min.Y < Other.Min.Y)
    {
        // Add Y min
        OutRemainder.Emplace(FVector(MinX, Min.Y, MinZ), FVector(MaxX, Other.Min.Y, MaxZ));
    }
    if (Other.Max.Y < Max.Y)
    {
        // Add Y max
        OutRemainder.Emplace(FVector(MinX, Other.Max.Y, MinZ), FVector(MaxX, Max.Y, MaxZ));
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox FVoxelBox::TransformBy(const FMatrix& Transform) const
{
	if (IsInfinite())
	{
		return Infinite;
	}

	const FVector P000 = Transform.TransformPosition(FVector(Min.X, Min.Y, Min.Z));
	const FVector P001 = Transform.TransformPosition(FVector(Max.X, Min.Y, Min.Z));
	const FVector P010 = Transform.TransformPosition(FVector(Min.X, Max.Y, Min.Z));
	const FVector P011 = Transform.TransformPosition(FVector(Max.X, Max.Y, Min.Z));
	const FVector P100 = Transform.TransformPosition(FVector(Min.X, Min.Y, Max.Z));
	const FVector P101 = Transform.TransformPosition(FVector(Max.X, Min.Y, Max.Z));
	const FVector P110 = Transform.TransformPosition(FVector(Min.X, Max.Y, Max.Z));
	const FVector P111 = Transform.TransformPosition(FVector(Max.X, Max.Y, Max.Z));

	FVoxelBox NewBox;

	NewBox.Min.X = FMath::Min3(P000.X, P001.X, FMath::Min3(P010.X, P011.X, FMath::Min3(P100.X, P101.X, FMath::Min(P110.X, P111.X))));
	NewBox.Min.Y = FMath::Min3(P000.Y, P001.Y, FMath::Min3(P010.Y, P011.Y, FMath::Min3(P100.Y, P101.Y, FMath::Min(P110.Y, P111.Y))));
	NewBox.Min.Z = FMath::Min3(P000.Z, P001.Z, FMath::Min3(P010.Z, P011.Z, FMath::Min3(P100.Z, P101.Z, FMath::Min(P110.Z, P111.Z))));

	NewBox.Max.X = FMath::Max3(P000.X, P001.X, FMath::Max3(P010.X, P011.X, FMath::Max3(P100.X, P101.X, FMath::Max(P110.X, P111.X))));
	NewBox.Max.Y = FMath::Max3(P000.Y, P001.Y, FMath::Max3(P010.Y, P011.Y, FMath::Max3(P100.Y, P101.Y, FMath::Max(P110.Y, P111.Y))));
	NewBox.Max.Z = FMath::Max3(P000.Z, P001.Z, FMath::Max3(P010.Z, P011.Z, FMath::Max3(P100.Z, P101.Z, FMath::Max(P110.Z, P111.Z))));

	return NewBox;
}

FVoxelBox FVoxelBox::TransformBy(const FTransform& Transform) const
{
	if (IsInfinite())
	{
		return Infinite;
	}

	const FVector P000 = Transform.TransformPosition(FVector(Min.X, Min.Y, Min.Z));
	const FVector P001 = Transform.TransformPosition(FVector(Max.X, Min.Y, Min.Z));
	const FVector P010 = Transform.TransformPosition(FVector(Min.X, Max.Y, Min.Z));
	const FVector P011 = Transform.TransformPosition(FVector(Max.X, Max.Y, Min.Z));
	const FVector P100 = Transform.TransformPosition(FVector(Min.X, Min.Y, Max.Z));
	const FVector P101 = Transform.TransformPosition(FVector(Max.X, Min.Y, Max.Z));
	const FVector P110 = Transform.TransformPosition(FVector(Min.X, Max.Y, Max.Z));
	const FVector P111 = Transform.TransformPosition(FVector(Max.X, Max.Y, Max.Z));

	FVoxelBox NewBox;

	NewBox.Min.X = FMath::Min3(P000.X, P001.X, FMath::Min3(P010.X, P011.X, FMath::Min3(P100.X, P101.X, FMath::Min(P110.X, P111.X))));
	NewBox.Min.Y = FMath::Min3(P000.Y, P001.Y, FMath::Min3(P010.Y, P011.Y, FMath::Min3(P100.Y, P101.Y, FMath::Min(P110.Y, P111.Y))));
	NewBox.Min.Z = FMath::Min3(P000.Z, P001.Z, FMath::Min3(P010.Z, P011.Z, FMath::Min3(P100.Z, P101.Z, FMath::Min(P110.Z, P111.Z))));

	NewBox.Max.X = FMath::Max3(P000.X, P001.X, FMath::Max3(P010.X, P011.X, FMath::Max3(P100.X, P101.X, FMath::Max(P110.X, P111.X))));
	NewBox.Max.Y = FMath::Max3(P000.Y, P001.Y, FMath::Max3(P010.Y, P011.Y, FMath::Max3(P100.Y, P101.Y, FMath::Max(P110.Y, P111.Y))));
	NewBox.Max.Z = FMath::Max3(P000.Z, P001.Z, FMath::Max3(P010.Z, P011.Z, FMath::Max3(P100.Z, P101.Z, FMath::Max(P110.Z, P111.Z))));

	return NewBox;
}

FVoxelBox FVoxelBox::TransformBy(const FTransform2d& Transform) const
{
	if (IsInfinite())
	{
		return Infinite;
	}

	const FVector2D P00 = Transform.TransformPoint(FVector2D(Min.X, Min.Y));
	const FVector2D P01 = Transform.TransformPoint(FVector2D(Max.X, Min.Y));
	const FVector2D P10 = Transform.TransformPoint(FVector2D(Min.X, Max.Y));
	const FVector2D P11 = Transform.TransformPoint(FVector2D(Max.X, Max.Y));

	FVoxelBox NewBox;

	NewBox.Min.X = FMath::Min3(P00.X, P01.X, FMath::Min(P10.X, P11.X));
	NewBox.Min.Y = FMath::Min3(P00.Y, P01.Y, FMath::Min(P10.Y, P11.Y));
	NewBox.Min.Z = Min.Z;

	NewBox.Max.X = FMath::Max3(P00.X, P01.X, FMath::Max(P10.X, P11.X));
	NewBox.Max.Y = FMath::Max3(P00.Y, P01.Y, FMath::Max(P10.Y, P11.Y));
	NewBox.Max.Z = Max.Z;

	return NewBox;
}

FVoxelBox FVoxelBox::InverseTransformBy(const FTransform& Transform) const
{
	if (IsInfinite())
	{
		return Infinite;
	}

	const FVector P000 = Transform.InverseTransformPosition(FVector(Min.X, Min.Y, Min.Z));
	const FVector P001 = Transform.InverseTransformPosition(FVector(Max.X, Min.Y, Min.Z));
	const FVector P010 = Transform.InverseTransformPosition(FVector(Min.X, Max.Y, Min.Z));
	const FVector P011 = Transform.InverseTransformPosition(FVector(Max.X, Max.Y, Min.Z));
	const FVector P100 = Transform.InverseTransformPosition(FVector(Min.X, Min.Y, Max.Z));
	const FVector P101 = Transform.InverseTransformPosition(FVector(Max.X, Min.Y, Max.Z));
	const FVector P110 = Transform.InverseTransformPosition(FVector(Min.X, Max.Y, Max.Z));
	const FVector P111 = Transform.InverseTransformPosition(FVector(Max.X, Max.Y, Max.Z));

	FVoxelBox NewBox;

	NewBox.Min.X = FMath::Min3(P000.X, P001.X, FMath::Min3(P010.X, P011.X, FMath::Min3(P100.X, P101.X, FMath::Min(P110.X, P111.X))));
	NewBox.Min.Y = FMath::Min3(P000.Y, P001.Y, FMath::Min3(P010.Y, P011.Y, FMath::Min3(P100.Y, P101.Y, FMath::Min(P110.Y, P111.Y))));
	NewBox.Min.Z = FMath::Min3(P000.Z, P001.Z, FMath::Min3(P010.Z, P011.Z, FMath::Min3(P100.Z, P101.Z, FMath::Min(P110.Z, P111.Z))));

	NewBox.Max.X = FMath::Max3(P000.X, P001.X, FMath::Max3(P010.X, P011.X, FMath::Max3(P100.X, P101.X, FMath::Max(P110.X, P111.X))));
	NewBox.Max.Y = FMath::Max3(P000.Y, P001.Y, FMath::Max3(P010.Y, P011.Y, FMath::Max3(P100.Y, P101.Y, FMath::Max(P110.Y, P111.Y))));
	NewBox.Max.Z = FMath::Max3(P000.Z, P001.Z, FMath::Max3(P010.Z, P011.Z, FMath::Max3(P100.Z, P101.Z, FMath::Max(P110.Z, P111.Z))));

	return NewBox;
}