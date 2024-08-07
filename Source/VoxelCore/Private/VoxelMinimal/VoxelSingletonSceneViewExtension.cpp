// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSingletonSceneViewExtension.h"

void FVoxelSingletonSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupViewFamily(InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupView(InViewFamily, InView);
	}
}

void FVoxelSingletonSceneViewExtension::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupViewPoint(Player, InViewInfo);
	}
}

void FVoxelSingletonSceneViewExtension::SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupViewProjectionMatrix(InOutProjectionData);
	}
}

void FVoxelSingletonSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->BeginRenderViewFamily(InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderView_RenderThread(GraphBuilder, InView);
	}
}

void FVoxelSingletonSceneViewExtension::PreInitViews_RenderThread(FRDGBuilder& GraphBuilder)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreInitViews_RenderThread(GraphBuilder);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, const bool bDepthBufferIsPopulated)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderBasePass_RenderThread(GraphBuilder, bDepthBufferIsPopulated);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderBasePassDeferred_RenderThread(GraphBuilder, InView, RenderTargets, SceneTextures);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderBasePassMobile_RenderThread(RHICmdList, InView);
	}
}

void FVoxelSingletonSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);
	}
}

void FVoxelSingletonSceneViewExtension::SubscribeToPostProcessingPass(const EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, const bool bIsPassEnabled)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SubscribeToPostProcessingPass(Pass, InOutPassCallbacks, bIsPassEnabled);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderView_RenderThread(GraphBuilder, InView);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderView_RenderThread(RHICmdList, InView);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderView_RenderThread(RHICmdList, InView);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderBasePass_RenderThread(RHICmdList, InView);
	}
}