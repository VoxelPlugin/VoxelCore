// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "SceneViewExtension.h"

class VOXELCORE_API FVoxelSingleton
{
public:
	FVoxelSingleton();
	virtual ~FVoxelSingleton();
	UE_NONCOPYABLE(FVoxelSingleton);

	virtual void Initialize() {}

	virtual void Tick() {}
	virtual void Tick_Async() {}
	virtual void Tick_RenderThread(FRHICommandList& RHICmdList) {}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
	virtual bool IsEditorOnly() const { return false; }

private:
	bool bIsRenderSingleton = false;

	friend class FVoxelRenderSingleton;
	friend class FVoxelSingletonManager;
};

class VOXELCORE_API FVoxelEditorSingleton : public FVoxelSingleton
{
public:
	using FVoxelSingleton::FVoxelSingleton;

	virtual bool IsEditorOnly() const final override
	{
		return true;
	}
};

class VOXELCORE_API FVoxelRenderSingleton : public FVoxelSingleton
{
public:
	FVoxelRenderSingleton();

	virtual void OnBeginFrame_RenderThread() {}
	virtual void OnEndFrame_RenderThread() {}

	virtual void SetupViewFamily_GameThread(FSceneViewFamily& ViewFamily) {}
	virtual void SetupView_GameThread(FSceneViewFamily& ViewFamily, FSceneView& View) {}
	virtual void SetupViewPoint_RenderThread(APlayerController* Player, FMinimalViewInfo& ViewInfo) {}
	virtual void SetupViewProjectionMatrix_RenderThread(FSceneViewProjectionData& InOutProjectionData) {}
	virtual void BeginRenderViewFamily_GameThread(FSceneViewFamily& ViewFamily) {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily) {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) {}
	virtual void PreInitViews_RenderThread(FRDGBuilder& GraphBuilder) {}
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View, bool bDepthBufferIsPopulated) {}
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) {}
	virtual void PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& View) {}
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) {}
	virtual void SubscribeToPostProcessingPass_RenderThread(ISceneViewExtension::EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) {}
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily) {}
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) {}
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) {}
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) {}
	virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) {}
};