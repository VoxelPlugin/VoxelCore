// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Containers/Queue.h"
#include "VoxelMinimal/VoxelVirtualStruct.h"
#include "VoxelMinimal/Containers/VoxelMap.h"
#include "VoxelMaterialRef.generated.h"

struct FVoxelMaterialInstanceRef;

USTRUCT()
struct VOXELCORE_API FVoxelDynamicMaterialParameter : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	virtual void Apply(FName Name, UMaterialInstanceDynamic& Instance) const VOXEL_PURE_VIRTUAL()
	virtual void AddOnChanged(const FSimpleDelegate& OnChanged) {}
};

class VOXELCORE_API FVoxelMaterialRef
	: public FVirtualDestructor
	, public TSharedFromThis<FVoxelMaterialRef>
{
public:
	static TSharedRef<FVoxelMaterialRef> Default();
	static TSharedRef<FVoxelMaterialRef> Make(UMaterialInterface* Material);
	// Will handle Parent being an instance
	static TSharedRef<FVoxelMaterialRef> MakeInstance(UMaterialInterface* Parent);

	static void RefreshInstance(UMaterialInstanceDynamic& Instance);

public:
	virtual ~FVoxelMaterialRef() override;
	UE_NONCOPYABLE(FVoxelMaterialRef);

	VOXEL_COUNT_INSTANCES();

	// Will be null if the asset is force deleted
	FORCEINLINE UMaterialInterface* GetMaterial() const
	{
		checkVoxelSlow(Material == WeakMaterial.Get() || !WeakMaterial.IsValid());
		return Material;
	}
	FORCEINLINE TWeakObjectPtr<UMaterialInterface> GetWeakMaterial() const
	{
		checkVoxelSlow(Material == WeakMaterial.Get() || !WeakMaterial.IsValid());
		return WeakMaterial;
	}
	// If true, this a material instance the plugin created & we can set parameters on it
	FORCEINLINE bool IsInstance() const
	{
		return MaterialInstanceRef.IsValid();
	}

	void AddResource(const TSharedPtr<FVirtualDestructor>& Resource);

	void SetScalarParameter_GameThread(FName Name, float Value);
	void SetVectorParameter_GameThread(FName Name, FVector4 Value);
	void SetTextureParameter_GameThread(FName Name, UTexture* Value);
	void SetDynamicParameter_GameThread(FName Name, const TSharedRef<FVoxelDynamicMaterialParameter>& Value);

	const float* FindScalarParameter(FName Name) const;
	const FVector4* FindVectorParameter(FName Name) const;
	UTexture* FindTextureParameter(FName Name) const;
	TSharedPtr<FVoxelDynamicMaterialParameter> FindDynamicParameter(FName Name) const;

private:
	FVoxelMaterialRef() = default;

	UMaterialInterface* Material = nullptr;
	TWeakObjectPtr<UMaterialInterface> WeakMaterial;
	TSharedPtr<FVoxelMaterialInstanceRef> MaterialInstanceRef;

	TVoxelMap<FName, float> ScalarParameters;
	TVoxelMap<FName, FVector4> VectorParameters;
	TVoxelMap<FName, TWeakObjectPtr<UTexture>> TextureParameters;
	TVoxelMap<FName, TSharedPtr<FVoxelDynamicMaterialParameter>> DynamicParameters;

	TQueue<TSharedPtr<FVirtualDestructor>, EQueueMode::Mpsc> Resources;

	friend class FVoxelMaterialRefManager;
};