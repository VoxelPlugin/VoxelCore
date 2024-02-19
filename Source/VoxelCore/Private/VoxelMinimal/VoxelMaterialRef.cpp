// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "MaterialDomain.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

DECLARE_VOXEL_COUNTER(VOXELCORE_API, STAT_VoxelNumMaterialInstancesPooled, "Num Material Instances Pooled");
DECLARE_VOXEL_COUNTER(VOXELCORE_API, STAT_VoxelNumMaterialInstancesUsed, "Num Material Instances Used");

DEFINE_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesPooled);
DEFINE_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesUsed);

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelMaterialRef);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelMaterialInstanceRef
{
	TObjectPtr<UMaterialInstanceDynamic> Instance;
};

struct FVoxelInstancePool
{
	double LastAccessTime = 0;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Instances;
};

class FVoxelMaterialRefManager : public FVoxelSingleton
{
public:
	TSharedPtr<FVoxelMaterialRef> DefaultMaterial;

	TVoxelArray<TWeakPtr<FVoxelMaterialRef>> MaterialRefs;
	TVoxelSet<TSharedPtr<FVoxelMaterialInstanceRef>> MaterialInstanceRefs;

	// We don't want to keep the parents alive
	TVoxelMap<TWeakObjectPtr<UMaterialInterface>, FVoxelInstancePool> InstancePools;
	TVoxelMap<TWeakObjectPtr<UMaterialInstanceDynamic>, TVoxelArray<TWeakPtr<FVoxelMaterialRef>>> InstanceToChildren;

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

		MaterialRefs.RemoveAllSwap([](const TWeakPtr<FVoxelMaterialRef>& MaterialRef)
		{
			return !MaterialRef.IsValid();
		});

		// Cleanup pools before tracking refs
		{
			const double AccessTimeThreshold = FPlatformTime::Seconds() - 30.f;
			for (auto It = InstancePools.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid() || It.Value().LastAccessTime < AccessTimeThreshold)
				{
					DEC_VOXEL_COUNTER_BY(STAT_VoxelNumMaterialInstancesPooled, It.Value().Instances.Num());
					It.RemoveCurrent();
				}
			}
		}

		for (const TWeakPtr<FVoxelMaterialRef>& WeakMaterial : MaterialRefs)
		{
			if (const TSharedPtr<FVoxelMaterialRef> Material = WeakMaterial.Pin())
			{
				Collector.AddReferencedObject(Material->Material);
			}
		}
		for (const TSharedPtr<FVoxelMaterialInstanceRef>& MaterialInstanceRef : MaterialInstanceRefs)
		{
			Collector.AddReferencedObject(MaterialInstanceRef->Instance);
		}

		for (auto& It : InstancePools)
		{
			Collector.AddReferencedObjects(It.Value.Instances);
		}
	}
	//~ End FVoxelSingleton Interface

public:
	UMaterialInstanceDynamic* GetInstanceFromPool(UMaterialInterface* Parent)
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		INC_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesUsed);

		UMaterialInstanceDynamic* Instance = nullptr;

		// Create the pool if needed so that LastAccessTime is properly set
		// Otherwise instances returned to the pool can get incorrectly immediately GCed
		FVoxelInstancePool& Pool = InstancePools.FindOrAdd(Parent);
		Pool.LastAccessTime = FPlatformTime::Seconds();

		while (!Instance && Pool.Instances.Num() > 0)
		{
			Instance = Pool.Instances.Pop(false);
			DEC_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesPooled);
		}

		if (!Instance)
		{
			Instance = UMaterialInstanceDynamic::Create(Parent, GetTransientPackage());
		}

		check(Instance);
		return Instance;
	}
	void ReturnInstanceToPool(UMaterialInstanceDynamic* Instance)
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		DEC_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesUsed);

		if (GExitPurge ||
			!IsValid(Instance) ||
			!IsValid(Instance->Parent))
		{
			// Instance won't be valid on exit
			return;
		}

		Instance->ClearParameterValues();

		InstancePools.FindOrAdd(Instance->Parent).Instances.Add(Instance);
		INC_VOXEL_COUNTER(STAT_VoxelNumMaterialInstancesPooled);
	}
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

	const TSharedRef<FVoxelMaterialRef> MaterialRef = MakeVoxelShareable(new (GVoxelMemory) FVoxelMaterialRef());
	MaterialRef->Material = Material;
	MaterialRef->WeakMaterial = Material;
	GVoxelMaterialRefManager->MaterialRefs.Add(MaterialRef);

	return MaterialRef;
}

TSharedRef<FVoxelMaterialRef> FVoxelMaterialRef::MakeInstance(UMaterialInterface* Parent)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!IsValid(Parent) ||
		!ensure(!Parent->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)))
	{
		Parent = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	// UMaterialInstanceDynamic cannot be parented to UMaterialInstanceDynamic
	// UMaterialInstanceDynamic can be parented to UMaterialInstanceConstant
	UMaterialInstanceDynamic* ParentInstance = Cast<UMaterialInstanceDynamic>(Parent);
	if (ParentInstance)
	{
		Parent = ParentInstance->Parent;
	}

	UMaterialInstanceDynamic* Instance = GVoxelMaterialRefManager->GetInstanceFromPool(Parent);
	check(Instance);

	if (ParentInstance)
	{
		VOXEL_SCOPE_COUNTER("CopyParameterOverrides");
		Instance->CopyParameterOverrides(ParentInstance);
	}

	const TSharedRef<FVoxelMaterialInstanceRef> MaterialInstanceRef = MakeVoxelShared<FVoxelMaterialInstanceRef>();
	MaterialInstanceRef->Instance = Instance;
	GVoxelMaterialRefManager->MaterialInstanceRefs.Add(MaterialInstanceRef);

	const TSharedRef<FVoxelMaterialRef> MaterialRef = MakeVoxelShareable(new (GVoxelMemory) FVoxelMaterialRef());
	MaterialRef->Material = Instance;
	MaterialRef->WeakMaterial = Instance;
	MaterialRef->MaterialInstanceRef = MaterialInstanceRef;
	GVoxelMaterialRefManager->MaterialRefs.Add(MaterialRef);

	if (ParentInstance)
	{
		VOXEL_SCOPE_COUNTER("InstanceToChildren");

		for (auto It = GVoxelMaterialRefManager->InstanceToChildren.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}

		TVoxelArray<TWeakPtr<FVoxelMaterialRef>>& Array = GVoxelMaterialRefManager->InstanceToChildren.FindOrAdd(ParentInstance);
		Array.RemoveAllSwap([](const TWeakPtr<FVoxelMaterialRef>& WeakMaterialRef)
		{
			return !WeakMaterialRef.IsValid();
		});
		Array.Add(MaterialRef);
	}

	return MaterialRef;
}

void FVoxelMaterialRef::RefreshInstance(UMaterialInstanceDynamic& Instance)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	const TVoxelArray<TWeakPtr<FVoxelMaterialRef>>* ChildrenPtr = GVoxelMaterialRefManager->InstanceToChildren.Find(TWeakObjectPtr<UMaterialInstanceDynamic>(&Instance));
	if (!ChildrenPtr)
	{
		return;
	}

	for (const TWeakPtr<FVoxelMaterialRef>& WeakMaterialRef : *ChildrenPtr)
	{
		const TSharedPtr<FVoxelMaterialRef> MaterialRef = WeakMaterialRef.Pin();
		if (!MaterialRef)
		{
			continue;
		}

		UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(MaterialRef->GetMaterial());
		if (!ensure(MaterialInstance))
		{
			continue;
		}

		VOXEL_SCOPE_COUNTER("RefreshInstance");

		MaterialInstance->CopyParameterOverrides(&Instance);

		for (const auto& It : MaterialRef->ScalarParameters)
		{
			MaterialInstance->SetScalarParameterValue(It.Key, It.Value);
		}
		for (const auto& It : MaterialRef->VectorParameters)
		{
			MaterialInstance->SetVectorParameterValue(It.Key, It.Value);
		}
		for (const auto& It : MaterialRef->TextureParameters)
		{
			MaterialInstance->SetTextureParameterValue(It.Key, It.Value.Get());
		}
		for (const auto& It : MaterialRef->DynamicParameters)
		{
			It.Value->Apply(It.Key, *MaterialInstance);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMaterialRef::~FVoxelMaterialRef()
{
	if (!MaterialInstanceRef)
	{
		return;
	}

	RunOnGameThread([MaterialInstanceRef = MaterialInstanceRef]
	{
		check(IsInGameThread());

		ensure(GVoxelMaterialRefManager->MaterialInstanceRefs.Remove(MaterialInstanceRef));
		GVoxelMaterialRefManager->ReturnInstanceToPool(MaterialInstanceRef->Instance);
	});
}

void FVoxelMaterialRef::AddResource(const TSharedPtr<FVirtualDestructor>& Resource)
{
	Resources.Enqueue(Resource);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMaterialRef::SetScalarParameter_GameThread(const FName Name, const float Value)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(IsInstance()))
	{
		return;
	}

	ScalarParameters.Add_EnsureNew(Name, Value);

	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(GetMaterial());
	if (!ensure(Instance))
	{
		return;
	}

	Instance->SetScalarParameterValue(Name, Value);
}

void FVoxelMaterialRef::SetVectorParameter_GameThread(const FName Name, const FVector4 Value)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(IsInstance()))
	{
		return;
	}

	VectorParameters.Add_EnsureNew(Name, Value);

	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(GetMaterial());
	if (!ensure(Instance))
	{
		return;
	}

	Instance->SetVectorParameterValue(Name, Value);
}

void FVoxelMaterialRef::SetTextureParameter_GameThread(const FName Name, UTexture* Value)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(IsInstance()))
	{
		return;
	}

	TextureParameters.Add_EnsureNew(Name, Value);

	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(GetMaterial());
	if (!ensure(Instance))
	{
		return;
	}

	Instance->SetTextureParameterValue(Name, Value);
}

void FVoxelMaterialRef::SetDynamicParameter_GameThread(const FName Name, const TSharedRef<FVoxelDynamicMaterialParameter>& Value)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(IsInstance()))
	{
		return;
	}

	DynamicParameters.Add_EnsureNew(Name, Value);

	Value->AddOnChanged(MakeWeakPtrDelegate(this, [this, Name, WeakValue = MakeWeakPtr(Value)]
	{
		RunOnGameThread(MakeWeakPtrLambda(this, [this, Name, WeakValue]
		{
			const TSharedPtr<FVoxelDynamicMaterialParameter> PinnedValue = WeakValue.Pin();
			if (!ensure(PinnedValue))
			{
				return;
			}

			UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(GetMaterial());
			if (!ensure(Instance))
			{
				return;
			}

			PinnedValue->Apply(Name, *Instance);
		}));
	}));

	UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(GetMaterial());
	if (!ensure(Instance))
	{
		return;
	}

	Value->Apply(Name, *Instance);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const float* FVoxelMaterialRef::FindScalarParameter(const FName Name) const
{
	ensureVoxelSlow(IsInstance());
	return ScalarParameters.Find(Name);
}

const FVector4* FVoxelMaterialRef::FindVectorParameter(const FName Name) const
{
	ensureVoxelSlow(IsInstance());
	return VectorParameters.Find(Name);
}

UTexture* FVoxelMaterialRef::FindTextureParameter(const FName Name) const
{
	ensureVoxelSlow(IsInstance());
	const TWeakObjectPtr<UTexture>* TexturePtr = TextureParameters.Find(Name);
	if (!TexturePtr)
	{
		return nullptr;
	}

	ensure(TexturePtr->IsValid());
	return TexturePtr->Get();
}

TSharedPtr<FVoxelDynamicMaterialParameter> FVoxelMaterialRef::FindDynamicParameter(const FName Name) const
{
	ensureVoxelSlow(IsInstance());
	return DynamicParameters.FindRef(Name);
}