// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/VoxelObjectPtr.h"

// Keeps material references alive
// This is needed because material objects are re-instantiated in-place when recompiled
// ie the object pointer is the same but with a new serial number, invalidating any weak pointer to it
class VOXELCORE_API FVoxelMaterialRef
{
public:
	static TSharedRef<FVoxelMaterialRef> Default();
	static TSharedRef<FVoxelMaterialRef> Make(UMaterialInterface* Material);

	UE_NONCOPYABLE(FVoxelMaterialRef);

	VOXEL_COUNT_INSTANCES();

	// Will be null if the asset is force deleted
	FORCEINLINE UMaterialInterface* GetMaterial() const
	{
		checkVoxelSlow(!WeakMaterial.Resolve_Unsafe() || WeakMaterial.Resolve_Unsafe() == Material);
		return Material;
	}
	FORCEINLINE TVoxelObjectPtr<UMaterialInterface> GetWeakMaterial() const
	{
		return WeakMaterial;
	}

protected:
	FVoxelMaterialRef() = default;

	TObjectPtr<UMaterialInterface> Material;
	TVoxelObjectPtr<UMaterialInterface> WeakMaterial;

	friend class FVoxelMaterialRefManager;
};

class VOXELCORE_API FVoxelMaterialInstanceRef : public FVoxelMaterialRef
{
public:
	static TSharedRef<FVoxelMaterialInstanceRef> Make(UMaterialInstanceDynamic& Material);

	UMaterialInstanceDynamic* GetInstance() const;
};