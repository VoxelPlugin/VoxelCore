// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"
#include "VoxelDebugDrawerManager.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelFreezeDebugDraws, false,
	"voxel.FreezeDebugDraws",
	"Freeze timed debug draws so they never expire");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

	if (PrivateDrawGroup)
	{
		PrivateDrawGroup->AddDraw_AnyThread(
			bIsOneFrame,
			PrivateLifeTime == -1 ? MAX_dbl : (FPlatformTime::Seconds() + PrivateLifeTime),
			Draw);
		return;
	}

	if (const TSharedPtr<FVoxelDebugDrawGroup>& DrawGroup = FVoxelTaskScope::GetContext().DrawGroup)
	{
		DrawGroup->AddDraw_AnyThread(
			bIsOneFrame,
			PrivateLifeTime == -1 ? MAX_dbl : (FPlatformTime::Seconds() + PrivateLifeTime),
			Draw);
		return;
	}

	FVoxelDebugDrawerWorldManager::Get(World)->GetGlobalGroup_AnyThread()->AddDraw_AnyThread(
		bIsOneFrame,
		PrivateLifeTime == -1 ? MAX_dbl : (FPlatformTime::Seconds() + PrivateLifeTime),
		Draw);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelDebugDrawer& FVoxelDebugDrawer::Group(const TSharedPtr<FVoxelDebugDrawGroup>& DrawGroup)
{
	PrivateDrawGroup = DrawGroup;
	return *this;
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelDebugDrawGroup> FVoxelDebugDrawGroup::Create()
{
	return MakeShareable(new FVoxelDebugDrawGroup());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawGroup::Clear_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	Draws_RequiresLock.Empty();
}

void FVoxelDebugDrawGroup::AddDraw_AnyThread(
	const bool bIsOneFrame,
	const double EndTime,
	const TSharedRef<const FVoxelDebugDraw>& Draw)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	Draws_RequiresLock.Add(FDraw
		{
			bIsOneFrame,
			EndTime,
			Draw
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawGroup::PushGroup_AnyThread()
{
	FVoxelDebugDrawerWorldManager::Get(GVoxelDebugDrawerManager->DefaultWorld)->AddGroup_AnyThread(AsShared());
}

void FVoxelDebugDrawGroup::PushGroup_AnyThread(const TVoxelObjectPtr<const UWorld> World)
{
	FVoxelDebugDrawerWorldManager::Get(World)->AddGroup_AnyThread(AsShared());
}

void FVoxelDebugDrawGroup::PushGroup_AnyThread(const UWorld* World)
{
	FVoxelDebugDrawerWorldManager::Get(World)->AddGroup_AnyThread(AsShared());
}

void FVoxelDebugDrawGroup::PushGroup_EnsureNew_AnyThread()
{
	FVoxelDebugDrawerWorldManager::Get(GVoxelDebugDrawerManager->DefaultWorld)->AddGroup_EnsureNew_AnyThread(AsShared());
}

void FVoxelDebugDrawGroup::PushGroup_EnsureNew_AnyThread(const TVoxelObjectPtr<const UWorld> World)
{
	FVoxelDebugDrawerWorldManager::Get(World)->AddGroup_EnsureNew_AnyThread(AsShared());
}

void FVoxelDebugDrawGroup::PushGroup_EnsureNew_AnyThread(const UWorld* World)
{
	FVoxelDebugDrawerWorldManager::Get(World)->AddGroup_EnsureNew_AnyThread(AsShared());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawGroup::IterateDraws(
	const double Time,
	TVoxelArray<TSharedPtr<const FVoxelDebugDraw>>& OutDraws)
{
	VOXEL_SCOPE_LOCK(CriticalSection);

	for (int32 Index = 0; Index < Draws_RequiresLock.Num(); Index++)
	{
		const FDraw& Draw = Draws_RequiresLock[Index];

		// Always render at least once
		OutDraws.Add(Draw.Draw);

		if (Draw.bIsOneFrame ||
			(!GVoxelFreezeDebugDraws && Draw.EndTime < Time))
		{
			Draws_RequiresLock.RemoveAtSwap(Index);
			Index--;
		}
	}
}