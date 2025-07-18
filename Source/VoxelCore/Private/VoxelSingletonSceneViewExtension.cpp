// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSingletonSceneViewExtension.h"
#include "SceneView.h"

void FVoxelSingletonSceneViewExtension::OnBeginFrame_RenderThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());
	check(!CurrentFrameNumber.IsSet());

	static int32 FrameCounter = 0;
	CurrentFrameNumber = ++FrameCounter;

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->OnBeginFrame_RenderThread();
	}
}

void FVoxelSingletonSceneViewExtension::OnEndFrame_RenderThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	static int32 FrameCounter = 0;
	check(CurrentFrameNumber == ++FrameCounter);
	CurrentFrameNumber.Reset();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->OnEndFrame_RenderThread();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSingletonSceneViewExtension::SetupViewFamily(FSceneViewFamily& ViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupViewFamily(ViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::SetupView(FSceneViewFamily& ViewFamily, FSceneView& View)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupView(ViewFamily, View);
	}
}

void FVoxelSingletonSceneViewExtension::SetupViewPoint(APlayerController* Player, FMinimalViewInfo& ViewInfo)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SetupViewPoint(Player, ViewInfo);
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

void FVoxelSingletonSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& ViewFamily)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->BeginRenderViewFamily(ViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderViewFamily_RenderThread(GraphBuilder, ViewFamily);
	}

	ensure(CurrentViews.Num() == 0);
	CurrentViews.Reset();

	ensure(!CurrentViewFamily);
	CurrentViewFamily = &ViewFamily;
}

void FVoxelSingletonSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	if (View.Family != CurrentViewFamily)
	{
		// Not the main view, eg a view queued by the Water plugin
		return;
	}

	CurrentViews.Add(&View);

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreRenderView_RenderThread(GraphBuilder, View);
	}
}

void FVoxelSingletonSceneViewExtension::PreInitViews_RenderThread(FRDGBuilder& GraphBuilder)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PreInitViews_RenderThread(GraphBuilder);
	}
}

void FVoxelSingletonSceneViewExtension::PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, const bool bDepthBufferIsPopulated)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	ensureVoxelSlow(CurrentViews.Num() > 0);

	for (FSceneView* View : CurrentViews)
	{
		for (FVoxelRenderSingleton* Singleton : Singletons)
		{
			Singleton->PreRenderBasePass_RenderThread(GraphBuilder, *View, bDepthBufferIsPopulated);
		}
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View, const FRenderTargetBindingSlots& RenderTargets, const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderBasePassDeferred_RenderThread(GraphBuilder, View, RenderTargets, SceneTextures);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& View)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInParallelRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderBasePassMobile_RenderThread(RHICmdList, View);
	}
}

void FVoxelSingletonSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);
	}
}

void FVoxelSingletonSceneViewExtension::SubscribeToPostProcessingPass(const EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, const bool bIsPassEnabled)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->SubscribeToPostProcessingPass_RenderThread(Pass, InOutPassCallbacks, bIsPassEnabled);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	ensure(CurrentViewFamily == &ViewFamily);
	CurrentViewFamily = nullptr;

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderViewFamily_RenderThread(GraphBuilder, ViewFamily);
	}
}

void FVoxelSingletonSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());

	ensure(CurrentViews.Remove(&View) == 1);

	for (FVoxelRenderSingleton* Singleton : Singletons)
	{
		Singleton->PostRenderView_RenderThread(GraphBuilder, View);
	}
}