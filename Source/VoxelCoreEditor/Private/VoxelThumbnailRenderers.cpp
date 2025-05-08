// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelThumbnailRenderers.h"
#include "SceneView.h"
#include "SceneInterface.h"
#include "ClassIconFinder.h"
#include "TextureResource.h"
#include "ThumbnailHelpers.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

void UVoxelThumbnailRenderer::BeginDestroy()
{
	ThumbnailScene.Reset();

	Super::BeginDestroy();
}

void UVoxelThumbnailRenderer::Draw(
	UObject* Object,
	const int32 X,
	const int32 Y,
	const uint32 Width,
	const uint32 Height,
	FRenderTarget* RenderTarget,
	FCanvas* Canvas,
	const bool bAdditionalViewFamily)
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
	return MakeShared<FStaticMeshThumbnailScene>();
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

void UVoxelTextureThumbnailRenderer::GetThumbnailSize(
	UObject* Object,
	const float Zoom,
	uint32& OutWidth,
	uint32& OutHeight) const
{
	Super::GetThumbnailSize(
		GetTexture(Object),
		Zoom,
		OutWidth,
		OutHeight);
}

void UVoxelTextureThumbnailRenderer::Draw(
	UObject* Object,
	const int32 X,
	const int32 Y,
	const uint32 Width,
	const uint32 Height,
	FRenderTarget* Target,
	FCanvas* Canvas,
	const bool bAdditionalViewFamily)
{
	Super::Draw(GetTexture(Object), X, Y, Width, Height, Target, Canvas, bAdditionalViewFamily);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelTextureWithBackgroundRenderer::CanVisualizeAsset(UObject* Object)
{
	return GetAssetIcon(Object).bCustomIcon;
}

void UVoxelTextureWithBackgroundRenderer::Draw(UObject* Object, int32 X, int32 Y, const uint32 Width, const uint32 Height, FRenderTarget* Target, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	if (!WidgetRenderer)
	{
		WidgetRenderer = MakeShared<FWidgetRenderer>(true);
		check(WidgetRenderer);
	}

	const FVoxelAssetIcon AssetIcon = GetAssetIcon(Object);
	ensureVoxelSlow(AssetIcon.bCustomIcon);

	UTexture2D* BackgroundTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Voxel/EditorAssets/T_ThumbnailBackground.T_ThumbnailBackground"));
	ensure(BackgroundTexture);

	{
		TArray<UTexture*> Textures;
		if (BackgroundTexture)
		{
			Textures.Add(BackgroundTexture);
		}
		if (AssetIcon.Icon)
		{
			Textures.Add(AssetIcon.Icon);
		}

		FVoxelTextureUtilities::FullyLoadTextures(Textures);
	}

	TSharedPtr<SImage> BackgroundImage;
	if (BackgroundTexture)
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(BackgroundTexture);
		IconBrush.ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
		IconBrush.Tiling = ESlateBrushTileType::NoTile;
		IconBrush.DrawAs = ESlateBrushDrawType::Image;

		BackgroundImage =
			SNew(SImage)
			.Image(&IconBrush)
			.ColorAndOpacity(AssetIcon.Color);
	}
	else
	{
		BackgroundImage =
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Brushes.White"))
			.ColorAndOpacity(AssetIcon.Color);
	}

	const TSharedRef<SOverlay> Thumbnail =
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			BackgroundImage.ToSharedRef()
		];

	if (AssetIcon.Icon)
	{
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(AssetIcon.Icon);
		IconBrush.ImageSize = FVector2D(AssetIcon.Icon->GetSizeX(), AssetIcon.Icon->GetSizeY());
		IconBrush.Tiling = ESlateBrushTileType::NoTile;
		IconBrush.DrawAs = ESlateBrushDrawType::Image;

		Thumbnail->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(&IconBrush)
			.DesiredSizeOverride(FVector2D(Width / 1.5f, Height / 1.5f))
			.ColorAndOpacity(AssetIcon.IconColor)
		];
	}
	else
	{
		Thumbnail->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FClassIconFinder::FindThumbnailForClass(Object->GetClass()))
			.DesiredSizeOverride(FVector2D(Width / 1.5f, Height / 1.5f))
			.ColorAndOpacity(AssetIcon.IconColor)
		];
	}

	WidgetRenderer->DrawWidget(Target, Thumbnail, FVector2D(Width, Height), 0.f);
}

void UVoxelTextureWithBackgroundRenderer::BeginDestroy()
{
	WidgetRenderer = nullptr;

	Super::BeginDestroy();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelThumbnailScene::FVoxelThumbnailScene()
{
	bForceAllUsedMipsResident = false;
}

void FVoxelThumbnailScene::GetViewMatrixParameters(
	const float InFOVDegrees,
	FVector& OutOrigin,
	float& OutOrbitPitch,
	float& OutOrbitYaw,
	float& OutOrbitZoom) const
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