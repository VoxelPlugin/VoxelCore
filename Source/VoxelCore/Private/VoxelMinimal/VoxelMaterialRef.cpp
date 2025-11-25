// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "MaterialDomain.h"
#include "MeshUVChannelInfo.h"
#include "Materials/Material.h"
#include "Engine/StreamableRenderAsset.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelMaterialRef);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelMaterialRefManager : public FVoxelSingleton
{
public:
	TSharedPtr<FVoxelMaterialRef> DefaultMaterial;
	TVoxelArray<TWeakPtr<FVoxelMaterialRef>> MaterialRefs;

public:
	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		UMaterial* DefaultMaterialObject = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterialObject);
		DefaultMaterial = FVoxelMaterialRef::Make(DefaultMaterialObject);
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		VOXEL_FUNCTION_COUNTER();

		for (auto It = MaterialRefs.CreateIterator(); It; ++It)
		{
			const TSharedPtr<FVoxelMaterialRef> Material = It->Pin();
			if (!Material)
			{
				It.RemoveCurrentSwap();
				continue;
			}

			Collector.AddReferencedObject(Material->Material);
		}
	}
	//~ End FVoxelSingleton Interface
};
FVoxelMaterialRefManager* GVoxelMaterialRefManager = new FVoxelMaterialRefManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelMaterialRef> FVoxelMaterialRef::Default()
{
	return GVoxelMaterialRefManager->DefaultMaterial.ToSharedRef();
}

TSharedRef<FVoxelMaterialRef> FVoxelMaterialRef::Make(UMaterialInterface* Material)
{
	check(IsInGameThread());

	if (!Material)
	{
		return Default();
	}

	const TSharedRef<FVoxelMaterialRef> MaterialRef = MakeShareable(new FVoxelMaterialRef());
	MaterialRef->Material = Material;
	MaterialRef->WeakMaterial = Material;
	GVoxelMaterialRefManager->MaterialRefs.Add(MaterialRef);

	return MaterialRef;
}

void FVoxelMaterialRef::GetStreamingRenderAssetInfo(
	FStreamingTextureLevelContext& LevelContext,
	const FBoxSphereBounds& Bounds,
	const float ComponentScale,
	TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets)
{
	VOXEL_FUNCTION_COUNTER();

	// Ensure that we have correct textures for current Feature and Quality levels
	if (INLINE_LAMBDA
		{
			if (!FeatureLevel.IsSet() ||
				!QualityLevel.IsSet())
			{
				return true;
			}

			return
				FeatureLevel.GetValue() != LevelContext.GetFeatureLevel() ||
				QualityLevel.GetValue() != LevelContext.GetQualityLevel();
		})
	{
		VOXEL_SCOPE_COUNTER("Build texture refs [" + WeakMaterial.GetPathName() + "]");

		FeatureLevel = LevelContext.GetFeatureLevel();
		QualityLevel = LevelContext.GetQualityLevel();
		TextureStreamingRefs.Reset();

		static const FMeshUVChannelInfo UVChannelData(1.f);

		FPrimitiveMaterialInfo MaterialData;
		MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
		MaterialData.UVChannelData = &UVChannelData;
		MaterialData.Material = GetMaterial();

		LevelContext.ProcessMaterial(
			Bounds,
			MaterialData,
			ComponentScale,
			OutStreamingRenderAssets,
			false);

		for (const FStreamingRenderAssetPrimitiveInfo& Info : OutStreamingRenderAssets)
		{
			FVoxelTextureStreamingRef& Ref = TextureStreamingRefs.Emplace_GetRef();
			Ref.Texture = Info.RenderAsset;
			Ref.TexelFactor = Info.TexelFactor / ComponentScale;
		}
		return;
	}

	for (const FVoxelTextureStreamingRef& Ref : TextureStreamingRefs)
	{
		FStreamingRenderAssetPrimitiveInfo& Info = OutStreamingRenderAssets.AddDefaulted_GetRef();
		Info.RenderAsset = Ref.Texture.Resolve();
		Info.TexelFactor = Ref.TexelFactor * ComponentScale;
		Info.PackedRelativeBox = PackedRelativeBox_Identity;
		UnpackRelativeBox(Bounds, Info.PackedRelativeBox, Info.Bounds);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelMaterialInstanceRef> FVoxelMaterialInstanceRef::Make(UMaterialInstanceDynamic& Material)
{
	check(IsInGameThread());

	const TSharedRef<FVoxelMaterialInstanceRef> MaterialRef = MakeShareable(new FVoxelMaterialInstanceRef());
	MaterialRef->Material = &Material;
	MaterialRef->WeakMaterial = Material;
	GVoxelMaterialRefManager->MaterialRefs.Add(MaterialRef);

	return MaterialRef;
}

UMaterialInstanceDynamic* FVoxelMaterialInstanceRef::GetInstance() const
{
	return CastChecked<UMaterialInstanceDynamic>(GetMaterial(), ECastCheckedType::NullAllowed);
}