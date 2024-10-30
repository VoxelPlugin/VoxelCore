// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "DrawDebugHelpers.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, float, GVoxelDebugThicknessMultiplier, 1.f,
	"voxel.debug.ThicknessMultiplier",
	"");

FVoxelDebugDrawer::FVoxelDebugDrawer()
{
}

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
	Voxel::GameTask([State = PrivateState]
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

FVoxelDebugDrawer& FVoxelDebugDrawer::DrawPoint(const FVector& Position)
{
	PrivateState->Drawers.Add([=](const FState& State)
	{
		DrawDebugPoint(
			State.GetWorld(),
			Position,
			State.Thickness,
			State.Color.ToFColor(true),
			false,
			State.LifeTime,
			0);
	});

	return *this;
}

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
			State.Thickness * GVoxelDebugThicknessMultiplier);
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
	ensureVoxelSlow(IsInGameThread());

	if (PrivateWorld == FObjectKey())
	{
		return GWorld;
	}

	return CastEnsured<UWorld>(PrivateWorld.ResolveObjectPtr());
}