// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDebugDrawerWorldManager : public IVoxelWorldSubsystem
{
public:
	GENERATED_VOXEL_WORLD_SUBSYSTEM_BODY(FVoxelDebugDrawerWorldManager);

public:
	void Clear_AnyThread();

	void AddDraw_AnyThread(
		bool bIsOneFrame,
		double EndTime,
		const TSharedRef<const FVoxelDebugDraw>& Draw);

public:
	void RenderPoints_RenderThread(
		FRDGBuilder& GraphBuilder,
		FViewInfo& View);

	void RenderLines_RenderThread(
		FRDGBuilder& GraphBuilder,
		FViewInfo& View);

public:
	//~ Begin IVoxelWorldSubsystem Interface
	virtual void Tick() override;
	//~ End IVoxelWorldSubsystem Interface

private:
	FVoxelFuture Future;
	const UWorld* World_Unsafe = nullptr;

	FVoxelCriticalSection CriticalSection;

	struct FDraw
	{
		bool bIsOneFrame = false;
		double EndTime = 0;
		TSharedPtr<const FVoxelDebugDraw> Draw;
	};
	TVoxelArray<FDraw> Draws_RequiresLock;

	TRefCountPtr<FRDGPooledBuffer> PooledPointBuffer;
	TWeakPtr<const TVoxelArray<FVoxelDebugPoint>> UploadedPointsToRender;
	TSharedPtr<const TVoxelArray<FVoxelDebugPoint>> PointsToRender_RenderThread;

	TRefCountPtr<FRDGPooledBuffer> PooledLineBuffer;
	TWeakPtr<const TVoxelArray<FVoxelDebugLine>> UploadedLinesToRender;
	TSharedPtr<const TVoxelArray<FVoxelDebugLine>> LinesToRender_RenderThread;

	friend class FVoxelDebugDrawerManager;
};

class FVoxelDebugDrawerManager : public FVoxelRenderSingleton
{
public:
	TVoxelObjectPtr<UWorld> DefaultWorld;

	//~ Begin FVoxelRenderSingleton Interface
	virtual void Tick() override;

	virtual void PostRenderBasePassDeferred_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneView& View,
		const FRenderTargetBindingSlots& RenderTargets,
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
	//~ End FVoxelRenderSingleton Interface
};
extern FVoxelDebugDrawerManager* GVoxelDebugDrawerManager;