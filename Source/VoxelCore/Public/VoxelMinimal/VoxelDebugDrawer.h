// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/VoxelObjectPtr.h"

struct VOXELCORE_API FVoxelDebugPoint
{
	FVector3f Center = FVector3f(ForceInit);
	uint8 SizeInCm = 10;
	uint8 R = 0;
	uint8 G = 0;
	uint8 B = 0;
};
checkStatic(sizeof(FVoxelDebugPoint) == sizeof(FVector4f));

struct VOXELCORE_API FVoxelDebugLine
{
	FVector3f Start = FVector3f(ForceInit);
	float Padding = 0.f;
	FVector3f End = FVector3f(ForceInit);
	uint8 R = 0;
	uint8 G = 0;
	uint8 B = 0;
	uint8 A = 0;
};
checkStatic(sizeof(FVoxelDebugLine) == 2 * sizeof(FVector4f));

struct VOXELCORE_API FVoxelDebugDraw
{
	TVoxelChunkedArray<FVoxelDebugPoint> Points;
	TVoxelChunkedArray<FVoxelDebugLine> Lines;
};

class VOXELCORE_API FVoxelDebugDrawer
{
public:
	FVoxelDebugDrawer();
	explicit FVoxelDebugDrawer(TVoxelObjectPtr<const UWorld> World);
	explicit FVoxelDebugDrawer(const UWorld* World);
	~FVoxelDebugDrawer();
	UE_NONCOPYABLE(FVoxelDebugDrawer);

public:
	FVoxelDebugDrawer& Color(const FLinearColor& NewColor);
	FVoxelDebugDrawer& OneFrame();
	FVoxelDebugDrawer& LifeTime(float NewLifeTime);

public:
	FVoxelDebugDrawer& DrawPoint(
		const FVector& Position,
		uint8 SizeInCm = 10);

	template<typename T>
	FVoxelDebugDrawer& DrawPoint(
		const FVector& Position,
		T) = delete;

public:
	FVoxelDebugDrawer& DrawLine(
		const FVector& Start,
		const FVector& End);

	FVoxelDebugDrawer& DrawBox(
		const FVoxelBox& Box,
		const FMatrix& Transform);

	FVoxelDebugDrawer& DrawBox(
		const FVoxelBox& Box,
		const FTransform& Transform);

private:
	const TVoxelObjectPtr<const UWorld> World;
	bool bIsOneFrame = false;
	float PrivateLifeTime = -1;
	FColor PrivateColor = FColor::Red;
	const TSharedRef<FVoxelDebugDraw> Draw = MakeShared<FVoxelDebugDraw>();
};