// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "ThumbnailHelpers.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "VoxelThumbnailRenderers.generated.h"

class FStaticMeshThumbnailScene;

UCLASS(Abstract)
class VOXELCOREEDITOR_API UVoxelThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	//~ Begin UDefaultSizedThumbnailRenderer Interface
	virtual void BeginDestroy() override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) final override;
	//~ End UDefaultSizedThumbnailRenderer Interface

	virtual TSharedPtr<FThumbnailPreviewScene> CreateScene()
	{
		ensure(false);
		return nullptr;
	}

	virtual bool InitializeScene(UObject* Object) VOXEL_PURE_VIRTUAL({});
	virtual void ClearScene(UObject* Object) VOXEL_PURE_VIRTUAL();

	template<typename T>
	TSharedRef<T> GetScene() const
	{
		return StaticCastSharedPtr<T>(ThumbnailScene).ToSharedRef();
	}

private:
	TSharedPtr<FThumbnailPreviewScene> ThumbnailScene;
};

UCLASS(Abstract)
class VOXELCOREEDITOR_API UVoxelStaticMeshThumbnailRenderer : public UVoxelThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FThumbnailPreviewScene> CreateScene() override;
	virtual bool InitializeScene(UObject* Object) override;
	virtual void ClearScene(UObject* Object) override;

	virtual UStaticMesh* GetStaticMesh(UObject* Object, TArray<UMaterialInterface*>& OutMaterialOverrides) const
	{
		ensure(false);
		return nullptr;
	}
};

UCLASS(Abstract)
class VOXELCOREEDITOR_API UVoxelTextureThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_BODY()

public:
	//~ Begin UTextureThumbnailRenderer Interface
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const final override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Target, FCanvas* Canvas, bool bAdditionalViewFamily) final override;
	//~ End UTextureThumbnailRenderer Interface

	virtual UTexture* GetTexture(UObject* Object) const
	{
		ensure(false);
		return nullptr;
	}
};

UCLASS(Abstract)
class VOXELCOREEDITOR_API UVoxelTextureWithBackgroundRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

	//~ Begin UVoxelTextureThumbnailRenderer Interface
	virtual bool CanVisualizeAsset(UObject* Object) override { return true; }
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Target, FCanvas* Canvas, bool bAdditionalViewFamily) final override;
	virtual void BeginDestroy() override;
	//~ End UVoxelTextureThumbnailRenderer Interface

	virtual void GetTextureWithBackground(
		UObject* Object,
		UTexture2D*& OutTexture,
		FSlateColor& OutTextureColor,
		FSlateColor& OutColor) const {}

private:
	TSharedPtr<FWidgetRenderer> WidgetRenderer;
};

class VOXELCOREEDITOR_API FVoxelThumbnailScene : public FThumbnailPreviewScene
{
protected:
	FVoxelThumbnailScene();

	//~ Begin FThumbnailPreviewScene Interface
	virtual void GetViewMatrixParameters(float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;
	//~ End FThumbnailPreviewScene Interface

	virtual FBoxSphereBounds GetBounds() const VOXEL_PURE_VIRTUAL({});
	virtual float GetBoundsScale() const
	{
		return 1.f;
	}
};

#define DEFINE_VOXEL_THUMBNAIL_RENDERER(RendererClass, Class) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		UThumbnailManager::Get().RegisterCustomRenderer(Class::StaticClass(), RendererClass::StaticClass()); \
	}
