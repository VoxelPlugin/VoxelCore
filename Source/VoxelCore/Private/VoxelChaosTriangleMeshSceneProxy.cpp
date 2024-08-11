// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelChaosTriangleMeshSceneProxy.h"
#include "MeshBatch.h"
#include "RawIndexBuffer.h"
#include "SceneManagement.h"
#include "LocalVertexFactory.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Chaos/TriangleMeshImplicitObject.h"

class FVoxelChaosTriangleMeshRenderData
{
public:
	explicit FVoxelChaosTriangleMeshRenderData(const Chaos::FTriangleMeshImplicitObject& TriangleMesh)
	{
		VOXEL_FUNCTION_COUNTER();

		TVoxelArray<uint32> Indices;
		TVoxelArray<FVector3f> Vertices;
		{
			const auto Process = [&](auto Elements)
			{
				for (const auto& Element : Elements)
				{
					Indices.Add(Vertices.Num());
					Vertices.Add(TriangleMesh.Particles().GetX(Element[0]));

					Indices.Add(Vertices.Num());
					Vertices.Add(TriangleMesh.Particles().GetX(Element[1]));

					Indices.Add(Vertices.Num());
					Vertices.Add(TriangleMesh.Particles().GetX(Element[2]));
				}
			};

			if (TriangleMesh.Elements().RequiresLargeIndices())
			{
				Process(TriangleMesh.Elements().GetLargeIndexBuffer());
			}
			else
			{
				Process(TriangleMesh.Elements().GetSmallIndexBuffer());
			}
		}

		IndexBuffer.SetIndices(Indices.ToConstArray(), EIndexBufferStride::Force32Bit);
		PositionVertexBuffer.Init(Vertices.Num(), false);
		StaticMeshVertexBuffer.Init(Vertices.Num(), 1, false);
		ColorVertexBuffer.Init(Vertices.Num(), false);

		for (int32 Index = 0; Index < Vertices.Num(); Index++)
		{
			PositionVertexBuffer.VertexPosition(Index) = Vertices[Index];
			StaticMeshVertexBuffer.SetVertexUV(Index, 0, FVector2f::ZeroVector);
			ColorVertexBuffer.VertexColor(Index) = FColor::Black;
		}

		for (int32 Index = 0; Index < Vertices.Num(); Index += 3)
		{
			const FVector3f Normal = FVoxelUtilities::GetTriangleNormal(
				Vertices[Index + 0],
				Vertices[Index + 1],
				Vertices[Index + 2]);

			const FVector3f Tangent = FVector3f::ForwardVector;
			const FVector3f Bitangent = FVector3f::CrossProduct(Normal, Tangent);

			StaticMeshVertexBuffer.SetVertexTangents(Index + 0, Tangent, Bitangent, Normal);
			StaticMeshVertexBuffer.SetVertexTangents(Index + 1, Tangent, Bitangent, Normal);
			StaticMeshVertexBuffer.SetVertexTangents(Index + 2, Tangent, Bitangent, Normal);
		}

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		IndexBuffer.InitResource(RHICmdList);
		PositionVertexBuffer.InitResource(RHICmdList);
		StaticMeshVertexBuffer.InitResource(RHICmdList);
		ColorVertexBuffer.InitResource(RHICmdList);

		VertexFactory = MakeUnique<FLocalVertexFactory>(GMaxRHIFeatureLevel, "FVoxelLandscapeCollider_Chaos_RenderData");

		FLocalVertexFactory::FDataType Data;
		PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory.Get(), Data);
		StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory.Get(), Data);
		StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory.Get(), Data);
		ColorVertexBuffer.BindColorVertexBuffer(VertexFactory.Get(), Data);

		VertexFactory->SetData(RHICmdList, Data);
		VertexFactory->InitResource(RHICmdList);
	}
	~FVoxelChaosTriangleMeshRenderData()
	{
		VOXEL_FUNCTION_COUNTER();

		IndexBuffer.ReleaseResource();
		PositionVertexBuffer.ReleaseResource();
		StaticMeshVertexBuffer.ReleaseResource();
		ColorVertexBuffer.ReleaseResource();
		VertexFactory->ReleaseResource();
	}

	void Draw_RenderThread(FMeshBatch& MeshBatch) const
	{
		VOXEL_FUNCTION_COUNTER();

		MeshBatch.Type = PT_TriangleList;
		MeshBatch.VertexFactory = VertexFactory.Get();

		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer.GetNumIndices() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = PositionVertexBuffer.GetNumVertices() - 1;
	}

private:
	FRawStaticIndexBuffer IndexBuffer{ false };
	FPositionVertexBuffer PositionVertexBuffer;
	FStaticMeshVertexBuffer StaticMeshVertexBuffer;
	FColorVertexBuffer ColorVertexBuffer;
	TUniquePtr<FLocalVertexFactory> VertexFactory;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelChaosTriangleMeshSceneProxy::FVoxelChaosTriangleMeshSceneProxy(
	const UPrimitiveComponent& Component,
	const TRefCountPtr<Chaos::FTriangleMeshImplicitObject>& TriangleMesh)
	: FPrimitiveSceneProxy(&Component)
	, TriangleMesh(TriangleMesh)
{
	// We create render data on-demand, can't be on a background thread
	bSupportsParallelGDME = false;
}

void FVoxelChaosTriangleMeshSceneProxy::DestroyRenderThreadResources()
{
	VOXEL_FUNCTION_COUNTER();
	RenderData.Reset();
}

void FVoxelChaosTriangleMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, const uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!RenderData)
	{
		RenderData = MakeVoxelShared<FVoxelChaosTriangleMeshRenderData>(*TriangleMesh);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		ensure(
			Views[ViewIndex]->Family->EngineShowFlags.Collision ||
			Views[ViewIndex]->Family->EngineShowFlags.CollisionPawn ||
			Views[ViewIndex]->Family->EngineShowFlags.CollisionVisibility);

		FMeshBatch& MeshBatch = Collector.AllocateMesh();
		MeshBatch.MaterialRenderProxy = FVoxelUtilities::CreateColoredMaterialRenderProxy(Collector, GetWireframeColor());
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.bDisableBackfaceCulling = true;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		RenderData->Draw_RenderThread(MeshBatch);

		Collector.AddMesh(ViewIndex, MeshBatch);
	}
}

FPrimitiveViewRelevance FVoxelChaosTriangleMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = true;
	Result.bRenderInMainPass = true;
	Result.bDynamicRelevance =
			View->Family->EngineShowFlags.Collision ||
			View->Family->EngineShowFlags.CollisionPawn ||
			View->Family->EngineShowFlags.CollisionVisibility;
	return Result;
}

uint32 FVoxelChaosTriangleMeshSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

SIZE_T FVoxelChaosTriangleMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}