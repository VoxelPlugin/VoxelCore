// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelMaterialInstanceRef> FVoxelMaterialInstanceRef::Make(UMaterialInstanceDynamic& Material)
{
	check(IsInGameThread());

	const TSharedRef<FVoxelMaterialInstanceRef> MaterialRef = MakeShareable(new FVoxelMaterialInstanceRef());
	MaterialRef->Material = Material;
	MaterialRef->WeakMaterial = Material;
	GVoxelMaterialRefManager->MaterialRefs.Add(MaterialRef);

	return MaterialRef;
}

UMaterialInstanceDynamic* FVoxelMaterialInstanceRef::GetInstance() const
{
	return CastChecked<UMaterialInstanceDynamic>(GetMaterial(), ECastCheckedType::NullAllowed);
}