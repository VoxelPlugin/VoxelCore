// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PrimitiveSceneProxy.h"

class VOXELCORE_API FVoxelChaosTriangleMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	const TRefCountPtr<Chaos::FTriangleMeshImplicitObject> TriangleMesh;

	explicit FVoxelChaosTriangleMeshSceneProxy(
		const UPrimitiveComponent& Component,
		const TRefCountPtr<Chaos::FTriangleMeshImplicitObject>& TriangleMesh);

public:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual SIZE_T GetTypeHash() const override;
	//~ End FPrimitiveSceneProxy Interface

private:
	mutable TSharedPtr<class FVoxelChaosTriangleMeshRenderData> RenderData;
};