// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelDebugDrawerManager.h"

FVoxelDebugDrawer::FVoxelDebugDrawer()
	: World(GVoxelDebugDrawerManager->DefaultWorld)
{
}

FVoxelDebugDrawer::FVoxelDebugDrawer(const TVoxelObjectPtr<const UWorld> World)
	: World(World)
{
}

FVoxelDebugDrawer::FVoxelDebugDrawer(const UWorld* World)
	: FVoxelDebugDrawer(MakeVoxelObjectPtr(World))
{
}

FVoxelDebugDrawer::~FVoxelDebugDrawer()
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelDebugDrawerWorldManager::Get(World)->AddDraw_AnyThread(
		bIsOneFrame,
		PrivateLifeTime == -1 ? MAX_dbl : (FPlatformTime::Seconds() + PrivateLifeTime),
		Draw);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDebugDrawer& FVoxelDebugDrawer::Color(const FLinearColor& NewColor)
{
	PrivateColor = NewColor.ToFColor(false);
	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::OneFrame()
{
	bIsOneFrame = true;
	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::LifeTime(const float NewLifeTime)
{
	PrivateLifeTime = NewLifeTime;
	return *this;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawPoint(
	const FVector& Position,
	const uint8 SizeInCm)
{
	Draw->Points.Add(FVoxelDebugPoint
	{
		FVector3f(Position),
		SizeInCm,
		PrivateColor.R,
		PrivateColor.G,
		PrivateColor.B
	});

	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawLine(
	const FVector& Start,
	const FVector& End)
{
	Draw->Lines.Add(FVoxelDebugLine
	{
		FVector3f(Start),
		0.f,
		FVector3f(End),
		PrivateColor.R,
		PrivateColor.G,
		PrivateColor.B
	});

	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawBox(
	const FVoxelBox& Box,
	const FMatrix& Transform)
{
	VOXEL_FUNCTION_COUNTER();

	if (Box.IsInfinite())
	{
		return *this;
	}

	const auto Get = [&](const double X, const double Y, const double Z) -> FVector
	{
		return Transform.TransformPosition(FVector(X, Y, Z));
	};

	DrawLine(Get(Box.Min.X, Box.Min.Y, Box.Min.Z), Get(Box.Max.X, Box.Min.Y, Box.Min.Z));
	DrawLine(Get(Box.Min.X, Box.Max.Y, Box.Min.Z), Get(Box.Max.X, Box.Max.Y, Box.Min.Z));
	DrawLine(Get(Box.Min.X, Box.Min.Y, Box.Max.Z), Get(Box.Max.X, Box.Min.Y, Box.Max.Z));
	DrawLine(Get(Box.Min.X, Box.Max.Y, Box.Max.Z), Get(Box.Max.X, Box.Max.Y, Box.Max.Z));

	DrawLine(Get(Box.Min.X, Box.Min.Y, Box.Min.Z), Get(Box.Min.X, Box.Max.Y, Box.Min.Z));
	DrawLine(Get(Box.Max.X, Box.Min.Y, Box.Min.Z), Get(Box.Max.X, Box.Max.Y, Box.Min.Z));
	DrawLine(Get(Box.Min.X, Box.Min.Y, Box.Max.Z), Get(Box.Min.X, Box.Max.Y, Box.Max.Z));
	DrawLine(Get(Box.Max.X, Box.Min.Y, Box.Max.Z), Get(Box.Max.X, Box.Max.Y, Box.Max.Z));

	DrawLine(Get(Box.Min.X, Box.Min.Y, Box.Min.Z), Get(Box.Min.X, Box.Min.Y, Box.Max.Z));
	DrawLine(Get(Box.Max.X, Box.Min.Y, Box.Min.Z), Get(Box.Max.X, Box.Min.Y, Box.Max.Z));
	DrawLine(Get(Box.Min.X, Box.Max.Y, Box.Min.Z), Get(Box.Min.X, Box.Max.Y, Box.Max.Z));
	DrawLine(Get(Box.Max.X, Box.Max.Y, Box.Min.Z), Get(Box.Max.X, Box.Max.Y, Box.Max.Z));

	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawBox(
	const FVoxelBox& Box,
	const FTransform& Transform)
{
	return DrawBox(
		Box,
		Transform.ToMatrixWithScale());
}