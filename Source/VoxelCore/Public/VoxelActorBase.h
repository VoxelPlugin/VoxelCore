// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelActorBase.generated.h"

class FVoxelTransformRef;

class VOXELCORE_API IVoxelActorRuntime : public TSharedFromThis<IVoxelActorRuntime>
{
public:
	IVoxelActorRuntime() = default;
	virtual ~IVoxelActorRuntime() = default;

	FORCEINLINE bool IsDestroyed() const
	{
		return bPrivateIsDestroyed;
	}

	virtual void Destroy() {}
	virtual void Tick() {}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

private:
	bool bPrivateIsDestroyed = false;

	friend class AVoxelActorBase;
};

UCLASS(Within = VoxelActorBase)
class VOXELCORE_API UVoxelActorBaseRootComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UVoxelActorBaseRootComponent();

	//~ Begin UPrimitiveComponent Interface
	virtual void UpdateBounds() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent Interface
};

UCLASS(HideCategories = ("Rendering", "Replication", "Input", "Collision", "LOD", "HLOD", "Cooking", "DataLayers", "Networking", "Physics"))
class VOXELCORE_API AVoxelActorBase : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Voxel")
	bool bCreateOnBeginPlay = true;

#if WITH_EDITOR
	bool bCreateOnConstruction_EditorOnly = true;
#endif

	FSimpleMulticastDelegate OnRuntimeCreated;
	FSimpleMulticastDelegate OnRuntimeDestroyed;

public:
	AVoxelActorBase();
	virtual ~AVoxelActorBase() override;

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	//~ End AActor Interface

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	bool IsRuntimeCreated() const
	{
		return PrivateRuntime.IsValid();
	}

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void QueueRecreateRuntime()
	{
		bPrivateRecreateQueued = true;
	}

	// Will call Create when it's ready to be created
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void QueueCreateRuntime();

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void CreateRuntime();

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void DestroyRuntime();

	void FlushRecreateRuntime();

public:
	virtual bool CanBeCreated() const { return true; }
	virtual void NotifyTransformChanged() {}
	virtual void FixupProperties() {}
	virtual bool ShouldDestroyWhenHidden() const { return false; }
	virtual FVoxelBox GetLocalBounds() const { return {}; }

protected:
	TSharedPtr<IVoxelActorRuntime> GetActorRuntime() const
	{
		return PrivateRuntime;
	}

	virtual TSharedPtr<IVoxelActorRuntime> CreateNewRuntime() VOXEL_PURE_VIRTUAL({});

private:
	bool bPrivateCreateQueued = false;
	bool bPrivateRecreateQueued = false;
	bool bPrivateCreateOnceVisible = false;
	TSharedPtr<IVoxelActorRuntime> PrivateRuntime;
	TSharedPtr<FVoxelTransformRef> PrivateTransformRef;

public:
	template<typename T>
	T* NewComponent()
	{
		return CastChecked<T>(this->NewComponent(StaticClassFast<T>()), ECastCheckedType::NullAllowed);
	}

	USceneComponent* NewComponent(const UClass* Class);
	void RemoveComponent(USceneComponent* Component);

private:
	bool bPrivateDisableModify = false;
	TVoxelSet<TWeakObjectPtr<USceneComponent>> PrivateComponents;
	TVoxelMap<const UClass*, TVoxelArray<TWeakObjectPtr<USceneComponent>>> PrivateClassToWeakComponents;
};