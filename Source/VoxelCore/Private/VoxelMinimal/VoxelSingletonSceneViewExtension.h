// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelSingletonSceneViewExtension : public FSceneViewExtensionBase
{
public:
	using FSceneViewExtensionBase::FSceneViewExtensionBase;

	TVoxelArray<FVoxelRenderSingleton*> Singletons;

	//~ Begin FSceneViewExtensionBase Interface
	virtual void SetupViewFamily(FSceneViewFamily& ViewFamily) override;
	virtual void SetupView(FSceneViewFamily& ViewFamily, FSceneView& View) override;
	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& ViewInfo) override;
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& ViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) override;
	virtual void PreInitViews_RenderThread(FRDGBuilder& GraphBuilder) override;
#if 0
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) override;
#endif
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
	virtual void PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& View) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily) override;
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) override;
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) override;
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View) override;
	//~ End FSceneViewExtensionBase Interface

private:
	FSceneView* CurrentView = nullptr;
};