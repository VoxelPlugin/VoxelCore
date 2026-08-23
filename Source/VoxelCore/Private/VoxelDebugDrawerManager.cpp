// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelDebugDrawerManager.h"
#include "SceneRendering.h"
#include "SceneView.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.ClearDebugDraws",
	"Clear all debug draws")
{
	for (const TSharedRef<FVoxelDebugDrawerWorldManager>& Manager : FVoxelDebugDrawerWorldManager::GetAll())
	{
		Manager->ClearAll_AnyThread();
	}
}

FVoxelDebugDrawerManager* GVoxelDebugDrawerManager = new FVoxelDebugDrawerManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BEGIN_SHADER_PARAMETER_STRUCT(FVoxelDebugPointParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, PointDataBuffer)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVoxelDebugLineParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LineDataBuffer)
	SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [4])
END_SHADER_PARAMETER_STRUCT()

class FVoxelDebugPointVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVoxelDebugPointVS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelDebugPointVS, FGlobalShader);
	using FParameters = FVoxelDebugPointParameters;
};

class FVoxelDebugPointPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelDebugPointPS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelDebugPointPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVoxelDebugPointParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FVoxelDebugLineVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVoxelDebugLineVS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelDebugLineVS, FGlobalShader);
	using FParameters = FVoxelDebugLineParameters;
};

class FVoxelDebugLinePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelDebugLinePS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelDebugLinePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVoxelDebugLineParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVoxelDebugPointVS, "/Plugin/Voxel/VoxelDebugDraw.usf", "PointVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVoxelDebugPointPS, "/Plugin/Voxel/VoxelDebugDraw.usf", "PointPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FVoxelDebugLineVS, "/Plugin/Voxel/VoxelDebugDraw.usf", "LineVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVoxelDebugLinePS, "/Plugin/Voxel/VoxelDebugDraw.usf", "LinePS", SF_Pixel);

DECLARE_GPU_STAT(VoxelDebugDrawPoints);
DECLARE_GPU_STAT(VoxelDebugDrawLines);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawerWorldManager::ClearAll_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	for (auto It = Groups_RequiresLock.CreateIterator(); It; ++It)
	{
		const TSharedPtr<FVoxelDebugDrawGroup> Group = It->Pin();
		if (!Group)
		{
			It.RemoveCurrent();
			continue;
		}

		Group->Clear_AnyThread();
	}
}

void FVoxelDebugDrawerWorldManager::AddGroup_AnyThread(const TSharedRef<FVoxelDebugDrawGroup>& Group)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	Groups_RequiresLock.Add(Group);
}

void FVoxelDebugDrawerWorldManager::AddGroup_EnsureNew_AnyThread(const TSharedRef<FVoxelDebugDrawGroup>& Group)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	Groups_RequiresLock.Add_EnsureNew(Group);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawerWorldManager::RenderPoints_RenderThread(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	const bool bAlwaysOnTop)
{
	ensure(IsInRenderingThread());

	FBuffers& Buffers = bAlwaysOnTop ? AlwaysOnTopBuffers : DepthTestedBuffers;

	const int32 NumPoints = Buffers.PointsToRender_RenderThread ? Buffers.PointsToRender_RenderThread->Num() : 0;
	if (NumPoints == 0)
	{
		Buffers.PooledPointBuffer = {};
		return;
	}

	VOXEL_FUNCTION_COUNTER_NUM(NumPoints);

	if (!Buffers.PooledPointBuffer ||
		int64(Buffers.PooledPointBuffer->Desc.NumElements) < NumPoints)
	{
		VOXEL_SCOPE_COUNTER("Create buffer");

		const FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(FVoxelDebugPoint), NumPoints),
			TEXT("VoxelDebugDraw.Points"));

		// Persistent, so we extract it immediately
		Buffers.PooledPointBuffer = GraphBuilder.ConvertToExternalBuffer(Buffer);
	}

	const FRDGBufferRef PointBuffer = GraphBuilder.RegisterExternalBuffer(Buffers.PooledPointBuffer);

	if (Buffers.UploadedPointsToRender != Buffers.PointsToRender_RenderThread)
	{
		Buffers.UploadedPointsToRender = Buffers.PointsToRender_RenderThread;

		FVoxelUtilities::UploadBuffer(
			GraphBuilder,
			PointBuffer,
			Buffers.PointsToRender_RenderThread->View<uint8>(),
			MakeSharedVoidPtr(Buffers.PointsToRender_RenderThread));
	}

	const FSceneTextures& SceneTextures = View.GetSceneTextures();

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	FVoxelDebugPointPS::FParameters* Parameters = GraphBuilder.AllocParameters<FVoxelDebugPointPS::FParameters>();
	Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneTextures.Depth.Target,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	Parameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);

	if (SceneTextures.GBufferB)
	{
		Parameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.GBufferB, ERenderTargetLoadAction::ELoad);
	}

	Parameters->Common.View = View.ViewUniformBuffer;
	Parameters->Common.PointDataBuffer = GraphBuilder.CreateSRV(PointBuffer, PF_A32B32G32R32F);

	const TShaderMapRef<FVoxelDebugPointVS> VertexShader(ShaderMap);
	const TShaderMapRef<FVoxelDebugPointPS> PixelShader(ShaderMap);

#if VOXEL_ENGINE_VERSION >= 508
	RDG_EVENT_SCOPE_STAT(GraphBuilder, VoxelDebugDrawPoints, "VoxelDebugDrawPoints");
#else
	RDG_GPU_STAT_SCOPE(GraphBuilder, VoxelDebugDrawPoints);
#endif

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("VoxelDebugDraw Points"),
		Parameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, &View, Parameters, NumPoints, bAlwaysOnTop](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f,
				View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = bAlwaysOnTop
				? TStaticDepthStencilState<false, CF_Always>::GetRHI()
				: TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGB, CW_RGBA>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters->Common);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);

			RHICmdList.DrawIndexedPrimitive(
				GTwoTrianglesIndexBuffer.IndexBufferRHI,
				0,
				0,
				4,
				0,
				2,
				NumPoints);
		});
}

void FVoxelDebugDrawerWorldManager::RenderLines_RenderThread(
	FRDGBuilder& GraphBuilder,
	FViewInfo& View,
	const bool bAlwaysOnTop)
{
	ensure(IsInRenderingThread());

	FBuffers& Buffers = bAlwaysOnTop ? AlwaysOnTopBuffers : DepthTestedBuffers;

	const int32 NumLines = Buffers.LinesToRender_RenderThread ? Buffers.LinesToRender_RenderThread->Num() : 0;
	if (NumLines == 0)
	{
		Buffers.PooledLineBuffer = {};
		return;
	}

	VOXEL_FUNCTION_COUNTER_NUM(NumLines);

	if (!Buffers.PooledLineBuffer ||
		int64(Buffers.PooledLineBuffer->Desc.NumElements) < int64(NumLines) * 2) // 2 float4s per line
	{
		VOXEL_SCOPE_COUNTER("Create buffer");

		const FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), NumLines * 2),
			TEXT("VoxelDebugDraw.Lines"));

		// Persistent, so we extract it immediately
		Buffers.PooledLineBuffer = GraphBuilder.ConvertToExternalBuffer(Buffer);
	}

	const FRDGBufferRef LineBuffer = GraphBuilder.RegisterExternalBuffer(Buffers.PooledLineBuffer);

	if (Buffers.UploadedLinesToRender != Buffers.LinesToRender_RenderThread)
	{
		Buffers.UploadedLinesToRender = Buffers.LinesToRender_RenderThread;

		FVoxelUtilities::UploadBuffer(
			GraphBuilder,
			LineBuffer,
			Buffers.LinesToRender_RenderThread->View<uint8>(),
			MakeSharedVoidPtr(Buffers.LinesToRender_RenderThread));
	}

	const FSceneTextures& SceneTextures = View.GetSceneTextures();

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.FeatureLevel);

	FVoxelDebugLinePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVoxelDebugLinePS::FParameters>();
	Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
		SceneTextures.Depth.Target,
		ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthWrite_StencilWrite);

	Parameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);

	if (SceneTextures.GBufferB)
	{
		Parameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.GBufferB, ERenderTargetLoadAction::ELoad);
	}

	Parameters->Common.View = View.ViewUniformBuffer;
	Parameters->Common.LineDataBuffer = GraphBuilder.CreateSRV(LineBuffer, PF_A32B32G32R32F);

	if (!ensureVoxelSlow(View.ViewFrustum.Planes.Num() == 4))
	{
		return;
	}

	const FMatrix Matrix = UE_508_SWITCH(View.ViewMatrices.GetProjectionMatrix(), View.ViewMatrices.GetViewToClip());

	FPlane LeftPlane(ForceInit);
	FPlane RightPlane(ForceInit);
	FPlane TopPlane(ForceInit);
	FPlane BottomPlane(ForceInit);
	ensureVoxelSlow(Matrix.GetFrustumLeftPlane(LeftPlane));
	ensureVoxelSlow(Matrix.GetFrustumRightPlane(RightPlane));
	ensureVoxelSlow(Matrix.GetFrustumTopPlane(TopPlane));
	ensureVoxelSlow(Matrix.GetFrustumBottomPlane(BottomPlane));

	Parameters->Common.FrustumPlanes[0] = FVector4f(FVector4(LeftPlane));
	Parameters->Common.FrustumPlanes[1] = FVector4f(FVector4(RightPlane));
	Parameters->Common.FrustumPlanes[2] = FVector4f(FVector4(TopPlane));
	Parameters->Common.FrustumPlanes[3] = FVector4f(FVector4(BottomPlane));

	const TShaderMapRef<FVoxelDebugLineVS> VertexShader(ShaderMap);
	const TShaderMapRef<FVoxelDebugLinePS> PixelShader(ShaderMap);

#if VOXEL_ENGINE_VERSION >= 508
	RDG_EVENT_SCOPE_STAT(GraphBuilder, VoxelDebugDrawLines, "VoxelDebugDrawLines");
#else
	RDG_GPU_STAT_SCOPE(GraphBuilder, VoxelDebugDrawLines);
#endif

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("VoxelDebugDraw Lines"),
		Parameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, &View, Parameters, NumLines, bAlwaysOnTop](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f,
				View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = bAlwaysOnTop
				? TStaticDepthStencilState<false, CF_Always>::GetRHI()
				: TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGB, CW_RGBA>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters->Common);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);

			// Draw 2 triangles (6 vertices) per line
			RHICmdList.DrawIndexedPrimitive(
				GTwoTrianglesIndexBuffer.IndexBufferRHI,
				0,
				0,
				4,
				0,
				2,
				NumLines);
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawerWorldManager::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	World_Unsafe = GetWorld().Resolve_Ensured();

	// Read OriginLocation here on the game thread; the async copy below uses the captured value to
	// bring Absolute draws into the current world (so they stay aligned across world origin rebasing)
	const FVector OriginOffset = World_Unsafe ? FVector(World_Unsafe->OriginLocation) : FVector::ZeroVector;

	if (!Future.IsComplete())
	{
		return;
	}

	Future = Voxel::AsyncTask(MakeWeakPtrLambda(this, [this, OriginOffset]() -> FVoxelFuture
	{
		VOXEL_SCOPE_COUNTER_NUM("FVoxelDebugDrawerManager Cleanup", Groups_RequiresLock.Num());

		TVoxelArray<TSharedPtr<const FVoxelDebugDraw>> DrawsToRender;
		DrawsToRender.Reserve(Groups_RequiresLock.Num());

		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			const double Time = FPlatformTime::Seconds();

			for (auto It = Groups_RequiresLock.CreateIterator(); It; ++It)
			{
				const TSharedPtr<FVoxelDebugDrawGroup> Group = It->Pin();
				if (!Group)
				{
					It.RemoveCurrent();
					continue;
				}

				Group->IterateDraws(Time, DrawsToRender);
			}
		}

		// Split by bAlwaysOnTop: each bucket is a separate draw call with its own depth state
		TVoxelArray<TSharedPtr<const FVoxelDebugDraw>> DrawsPerBucket[2];
		for (const TSharedPtr<const FVoxelDebugDraw>& Draw : DrawsToRender)
		{
			DrawsPerBucket[Draw->bAlwaysOnTop ? 1 : 0].Add(Draw);
		}

		struct FBucket
		{
			bool bUpdatePoints = false;
			bool bUpdateLines = false;
			TSharedPtr<const TVoxelArray<FVoxelDebugPoint>> Points;
			TSharedPtr<const TVoxelArray<FVoxelDebugLine>> Lines;
		};
		FBucket Buckets[2];

		bool bAnyUpdate = false;

		for (int32 BucketIndex = 0; BucketIndex < 2; BucketIndex++)
		{
			const TVoxelArray<TSharedPtr<const FVoxelDebugDraw>>& Draws = DrawsPerBucket[BucketIndex];

			int64 NumPoints = 0;
			int64 NumLines = 0;
			for (const TSharedPtr<const FVoxelDebugDraw>& Draw : Draws)
			{
				NumPoints += Draw->Points.Num();
				NumLines += Draw->Lines.Num();
			}

			if (!ensure(NumPoints < MAX_int32) ||
				!ensure(NumLines < MAX_int32))
			{
				return {};
			}

			TVoxelArray<FVoxelDebugPoint> PointsToRender;
			TVoxelArray<FVoxelDebugLine> LinesToRender;

			FVoxelUtilities::SetNumFast(PointsToRender, NumPoints);
			FVoxelUtilities::SetNumFast(LinesToRender, NumLines);

			int32 PointIndex = 0;
			int32 LineIndex = 0;
			for (const TSharedPtr<const FVoxelDebugDraw>& Draw : Draws)
			{
				// Absolute draws are in world-origin independent space; bring them to the current world
				const FVector3f Offset =
					Draw->Space == EVoxelWorldSpace::Absolute
					? -FVector3f(OriginOffset)
					: FVector3f(ForceInit);
				const bool bHasOffset = !Offset.IsZero();

				if (Draw->Points.Num() > 0)
				{
					const TVoxelArrayView<FVoxelDebugPoint> Destination = PointsToRender.View().Slice(PointIndex, Draw->Points.Num());
					Draw->Points.CopyTo(Destination);

					if (bHasOffset)
					{
						for (FVoxelDebugPoint& Point : Destination)
						{
							Point.Center += Offset;
						}
					}

					PointIndex += Draw->Points.Num();
				}

				if (Draw->Lines.Num() > 0)
				{
					const TVoxelArrayView<FVoxelDebugLine> Destination = LinesToRender.View().Slice(LineIndex, Draw->Lines.Num());
					Draw->Lines.CopyTo(Destination);

					if (bHasOffset)
					{
						for (FVoxelDebugLine& Line : Destination)
						{
							Line.Start += Offset;
							Line.End += Offset;
						}
					}

					LineIndex += Draw->Lines.Num();
				}
			}

			const FBuffers& Buffers = BucketIndex == 1 ? AlwaysOnTopBuffers : DepthTestedBuffers;

			FBucket& Bucket = Buckets[BucketIndex];

			Bucket.bUpdatePoints =
				!Buffers.PointsToRender_RenderThread ||
				!FVoxelUtilities::Equal(*Buffers.PointsToRender_RenderThread, PointsToRender);

			Bucket.bUpdateLines =
				!Buffers.LinesToRender_RenderThread ||
				!FVoxelUtilities::Equal(*Buffers.LinesToRender_RenderThread, LinesToRender);

			bAnyUpdate |= Bucket.bUpdatePoints || Bucket.bUpdateLines;

			Bucket.Points = MakeSharedCopy(MoveTemp(PointsToRender));
			Bucket.Lines = MakeSharedCopy(MoveTemp(LinesToRender));
		}

		if (!bAnyUpdate)
		{
			return {};
		}

		return Voxel::RenderTask(MakeWeakPtrLambda(this, [this,
			PointsToRender0 = Buckets[0].Points,
			LinesToRender0 = Buckets[0].Lines,
			PointsToRender1 = Buckets[1].Points,
			LinesToRender1 = Buckets[1].Lines,
			bUpdatePoints0 = Buckets[0].bUpdatePoints,
			bUpdateLines0 = Buckets[0].bUpdateLines,
			bUpdatePoints1 = Buckets[1].bUpdatePoints,
			bUpdateLines1 = Buckets[1].bUpdateLines]
		{
			ensure(IsInRenderingThread());

			if (bUpdatePoints0)
			{
				DepthTestedBuffers.PointsToRender_RenderThread = PointsToRender0;
			}
			if (bUpdateLines0)
			{
				DepthTestedBuffers.LinesToRender_RenderThread = LinesToRender0;
			}

			if (bUpdatePoints1)
			{
				AlwaysOnTopBuffers.PointsToRender_RenderThread = PointsToRender1;
			}
			if (bUpdateLines1)
			{
				AlwaysOnTopBuffers.LinesToRender_RenderThread = LinesToRender1;
			}
		}));
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawerManager::Tick()
{
	DefaultWorld = GWorld.GetReference();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDebugDrawerManager::PostRenderBasePassDeferred_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneView& View,
	const FRenderTargetBindingSlots& RenderTargets,
	const TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInRenderingThread());

	if (!ensureVoxelSlow(View.bIsViewInfo))
	{
		return;
	}
	const UWorld* World_Unsafe = View.Family->Scene->GetWorld();

	for (const TSharedRef<FVoxelDebugDrawerWorldManager>& Manager : FVoxelDebugDrawerWorldManager::GetAll())
	{
		if (Manager->World_Unsafe == World_Unsafe)
		{
			// Depth tested first, so that the always-on-top draws end up on top of them
			Manager->RenderPoints_RenderThread(GraphBuilder, static_cast<FViewInfo&>(View), false);
			Manager->RenderLines_RenderThread(GraphBuilder, static_cast<FViewInfo&>(View), false);

			Manager->RenderPoints_RenderThread(GraphBuilder, static_cast<FViewInfo&>(View), true);
			Manager->RenderLines_RenderThread(GraphBuilder, static_cast<FViewInfo&>(View), true);
		}
	}
}