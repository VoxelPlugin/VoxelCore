// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelThumbnailRenderers.h"
#include "SceneView.h"
#include "SceneInterface.h"
#include "ThumbnailHelpers.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

void UVoxelThumbnailRenderer::BeginDestroy()
{
	ThumbnailScene.Reset();

	Super::BeginDestroy();
}

void UVoxelThumbnailRenderer::Draw(UObject* Object, const int32 X, const int32 Y, const uint32 Width, const uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, const bool bAdditionalViewFamily)
{
	if (!ThumbnailScene)
	{
		ThumbnailScene = CreateScene();
	}

	for (AStaticMeshActor* Actor : TActorRange<AStaticMeshActor>(ThumbnailScene->GetWorld()))
	{
		Actor->SetActorRotation(FRotator(0, 90, 0));
	}

	if (!InitializeScene(Object))
	{
		return;
	}

	ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		.SetTime(GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily)
	);

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));

	ClearScene(Object);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FThumbnailPreviewScene> UVoxelStaticMeshThumbnailRenderer::CreateScene()
{
	return MakeVoxelShared<FStaticMeshThumbnailScene>();
}

bool UVoxelStaticMeshThumbnailRenderer::InitializeScene(UObject* Object)
{
	TArray<UMaterialInterface*> MaterialOverrides;
	UStaticMesh* StaticMesh = GetStaticMesh(Object, MaterialOverrides);
	if (!IsValid(StaticMesh))
	{
		return false;
	}

	const TSharedRef<FStaticMeshThumbnailScene> StaticMeshScene = GetScene<FStaticMeshThumbnailScene>();
	StaticMeshScene->SetStaticMesh(StaticMesh);
	StaticMeshScene->SetOverrideMaterials(MaterialOverrides);
	return true;
}

void UVoxelStaticMeshThumbnailRenderer::ClearScene(UObject* Object)
{
	const TSharedRef<FStaticMeshThumbnailScene> StaticMeshScene = GetScene<FStaticMeshThumbnailScene>();

	StaticMeshScene->SetStaticMesh(nullptr);
	StaticMeshScene->SetOverrideMaterials({});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelTextureThumbnailRenderer::GetThumbnailSize(UObject* Object, const float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	Super::GetThumbnailSize(GetTexture(Object), Zoom, OutWidth, OutHeight);
}

void UVoxelTextureThumbnailRenderer::Draw(UObject* Object, const int32 X, const int32 Y, const uint32 Width, const uint32 Height, FRenderTarget* Target, FCanvas* Canvas, const bool bAdditionalViewFamily)
{
	Super::Draw(GetTexture(Object), X, Y, Width, Height, Target, Canvas, bAdditionalViewFamily);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelThumbnailScene::FVoxelThumbnailScene()
{
	bForceAllUsedMipsResident = false;
}

void FVoxelThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	const FBoxSphereBounds Bounds = GetBounds();
	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	const float HalfMeshSize = Bounds.SphereRadius * GetBoundsScale();
	const float BoundsZOffset = GetBoundsZOffset(Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	const USceneThumbnailInfo* ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}