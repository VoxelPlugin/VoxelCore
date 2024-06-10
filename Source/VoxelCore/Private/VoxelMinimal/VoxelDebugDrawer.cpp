// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "DrawDebugHelpers.h"

FVoxelDebugDrawer::FVoxelDebugDrawer(const FObjectKey& World)
{
	PrivateState->PrivateWorld = World;
}

FVoxelDebugDrawer::FVoxelDebugDrawer(const UWorld* World)
	: FVoxelDebugDrawer(MakeObjectKey(World))
{
}

FVoxelDebugDrawer::FVoxelDebugDrawer(const TWeakObjectPtr<UWorld>& World)
	: FVoxelDebugDrawer(MakeObjectKey(World))
{
}

FVoxelDebugDrawer::~FVoxelDebugDrawer()
{
	RunOnGameThread([State = PrivateState]
	{
		VOXEL_FUNCTION_COUNTER();

		if (!ensure(State->GetWorld()))
		{
			return;
		}

		for (const TVoxelUniqueFunction<void(const FState&)>& Drawer : State->Drawers)
		{
			Drawer(*State);
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawLine(
	const FVector& Start,
	const FVector& End)
{
	PrivateState->Drawers.Add([=](const FState& State)
	{
		DrawDebugLine(
			State.GetWorld(),
			Start,
			End,
			State.Color.ToFColor(true),
			false,
			State.LifeTime,
			0,
			State.Thickness);
	});

	return *this;
}

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawBox(
	const FVoxelBox& Box,
	const FMatrix& Transform,
	const bool bScaleBySize)
{
	VOXEL_FUNCTION_COUNTER();

	if (Box.IsInfinite())
	{
		return *this;
	}

	if (bScaleBySize)
	{
		PrivateState->Thickness *= Box.Size().GetAbsMax() / 10000.f;
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
	const FTransform& Transform,
	const bool bScaleBySize)
{
	return DrawBox(
		Box,
		Transform.ToMatrixWithScale(),
		bScaleBySize);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UWorld* FVoxelDebugDrawer::FState::GetWorld() const
{
	return CastEnsured<UWorld>(PrivateWorld.ResolveObjectPtr());
}