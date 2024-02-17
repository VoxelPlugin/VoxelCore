// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelBox.h"
#include "VoxelMinimal/VoxelUniqueFunction.h"

class VOXELCORE_API FVoxelDebugDrawer
{
public:
	explicit FVoxelDebugDrawer(const FObjectKey& World);
	~FVoxelDebugDrawer();

public:
	FVoxelDebugDrawer& Color(const FLinearColor& NewColor)
	{
		PrivateState->Color = NewColor;
		return *this;
	}
	FVoxelDebugDrawer& Thickness(const float NewThickness)
	{
		PrivateState->Thickness = NewThickness;
		return *this;
	}
	FVoxelDebugDrawer& LifeTime(const float NewLifeTime)
	{
		PrivateState->LifeTime = NewLifeTime;
		return *this;
	}
	FVoxelDebugDrawer& OneFrame()
	{
		PrivateState->LifeTime = -1;
		return *this;
	}

public:
	FVoxelDebugDrawer& DrawLine(
		const FVector& Start,
		const FVector& End);

	FVoxelDebugDrawer& DrawBox(
		const FVoxelBox& Box,
		const FMatrix& Transform,
		bool bScaleBySize = true);

	FVoxelDebugDrawer& DrawBox(
		const FVoxelBox& Box,
		const FTransform& Transform,
		bool bScaleBySize = true);

private:
	struct FState
	{
		FObjectKey PrivateWorld;
		FLinearColor Color = FLinearColor::Red;
		float Thickness = 10.f;
		float LifeTime = 10.f;
		TVoxelArray<TVoxelUniqueFunction<void(const FState&)>> Drawers;

		UWorld* GetWorld() const;
	};
	const TSharedRef<FState> PrivateState = MakeVoxelShared<FState>();
};