// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPrimitiveComponentSettings.h"

void FVoxelPrimitiveComponentSettings::ApplyToComponent(UPrimitiveComponent& Component) const
{
	VOXEL_FUNCTION_COUNTER();

#define COPY(VariableName) \
	Component.VariableName = VariableName;

	Component.CastShadow = bCastShadow;
	COPY(IndirectLightingCacheQuality);
#if VOXEL_ENGINE_VERSION >= 505
	Component.SetLightmapType(LightmapType);
#else
	COPY(LightmapType);
#endif
	COPY(bEmissiveLightSource);
	COPY(bAffectDynamicIndirectLighting);
	COPY(bAffectIndirectLightingWhileHidden);
	COPY(bAffectDistanceFieldLighting);
	COPY(bCastDynamicShadow);
	COPY(bCastStaticShadow);
	COPY(ShadowCacheInvalidationBehavior);
	COPY(bCastVolumetricTranslucentShadow);
	COPY(bCastContactShadow);
	COPY(bSelfShadowOnly);
	COPY(bCastFarShadow);
	COPY(bCastInsetShadow);
	COPY(bCastCinematicShadow);
	COPY(bCastHiddenShadow);
	COPY(bCastShadowAsTwoSided);
	COPY(bLightAttachmentsAsGroup);
	COPY(bExcludeFromLightAttachmentGroup);
	COPY(bSingleSampleShadowFromStationaryLights);
	COPY(LightingChannels);
	COPY(bVisibleInReflectionCaptures);
	COPY(bVisibleInRealTimeSkyCaptures);
	COPY(bVisibleInRayTracing);
	COPY(bRenderInMainPass);
	COPY(bRenderInDepthPass);
	COPY(bReceivesDecals);
	COPY(bOwnerNoSee);
	COPY(bOnlyOwnerSee);
	COPY(bTreatAsBackgroundForOcclusion);
	COPY(bUseAsOccluder);
	COPY(bRenderCustomDepth);
	COPY(CustomDepthStencilValue);
	COPY(bVisibleInSceneCaptureOnly);
	COPY(bHiddenInSceneCapture);
	COPY(TranslucencySortPriority);
	COPY(TranslucencySortDistanceOffset);
	COPY(CustomDepthStencilWriteMask);
	COPY(RuntimeVirtualTextures);
	COPY(VirtualTextureLodBias);
	COPY(VirtualTextureCullMips);
	COPY(VirtualTextureMinCoverage);
	COPY(VirtualTextureRenderPassType);

#undef COPY

	Component.MarkRenderStateDirty();
}

bool FVoxelPrimitiveComponentSettings::operator==(const FVoxelPrimitiveComponentSettings& Other) const
{
	return
		bCastShadow == Other.bCastShadow &&
		IndirectLightingCacheQuality == Other.IndirectLightingCacheQuality &&
		LightmapType == Other.LightmapType &&
		bEmissiveLightSource == Other.bEmissiveLightSource &&
		bAffectDynamicIndirectLighting == Other.bAffectDynamicIndirectLighting &&
		bAffectIndirectLightingWhileHidden == Other.bAffectIndirectLightingWhileHidden &&
		bAffectDistanceFieldLighting == Other.bAffectDistanceFieldLighting &&
		bCastDynamicShadow == Other.bCastDynamicShadow &&
		bCastStaticShadow == Other.bCastStaticShadow &&
		ShadowCacheInvalidationBehavior == Other.ShadowCacheInvalidationBehavior &&
		bCastVolumetricTranslucentShadow == Other.bCastVolumetricTranslucentShadow &&
		bCastContactShadow == Other.bCastContactShadow &&
		bSelfShadowOnly == Other.bSelfShadowOnly &&
		bCastFarShadow == Other.bCastFarShadow &&
		bCastInsetShadow == Other.bCastInsetShadow &&
		bCastCinematicShadow == Other.bCastCinematicShadow &&
		bCastHiddenShadow == Other.bCastHiddenShadow &&
		bCastShadowAsTwoSided == Other.bCastShadowAsTwoSided &&
		bLightAttachmentsAsGroup == Other.bLightAttachmentsAsGroup &&
		bExcludeFromLightAttachmentGroup == Other.bExcludeFromLightAttachmentGroup &&
		bSingleSampleShadowFromStationaryLights == Other.bSingleSampleShadowFromStationaryLights &&
		LightingChannels.bChannel0 == Other.LightingChannels.bChannel0 &&
		LightingChannels.bChannel1 == Other.LightingChannels.bChannel1 &&
		LightingChannels.bChannel2 == Other.LightingChannels.bChannel2 &&
		bVisibleInReflectionCaptures == Other.bVisibleInReflectionCaptures &&
		bVisibleInRealTimeSkyCaptures == Other.bVisibleInRealTimeSkyCaptures &&
		bVisibleInRayTracing == Other.bVisibleInRayTracing &&
		bRenderInMainPass == Other.bRenderInMainPass &&
		bRenderInDepthPass == Other.bRenderInDepthPass &&
		bReceivesDecals == Other.bReceivesDecals &&
		bOwnerNoSee == Other.bOwnerNoSee &&
		bOnlyOwnerSee == Other.bOnlyOwnerSee &&
		bTreatAsBackgroundForOcclusion == Other.bTreatAsBackgroundForOcclusion &&
		bUseAsOccluder == Other.bUseAsOccluder &&
		bRenderCustomDepth == Other.bRenderCustomDepth &&
		CustomDepthStencilValue == Other.CustomDepthStencilValue &&
		bVisibleInSceneCaptureOnly == Other.bVisibleInSceneCaptureOnly &&
		bHiddenInSceneCapture == Other.bHiddenInSceneCapture &&
		TranslucencySortPriority == Other.TranslucencySortPriority &&
		TranslucencySortDistanceOffset == Other.TranslucencySortDistanceOffset &&
		CustomDepthStencilWriteMask == Other.CustomDepthStencilWriteMask &&
		RuntimeVirtualTextures == Other.RuntimeVirtualTextures &&
		VirtualTextureLodBias == Other.VirtualTextureLodBias &&
		VirtualTextureCullMips == Other.VirtualTextureCullMips &&
		VirtualTextureMinCoverage == Other.VirtualTextureMinCoverage &&
		VirtualTextureRenderPassType == Other.VirtualTextureRenderPassType;
}