﻿// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelActorBase.h"
#include "VoxelTransformRef.h"
#include "Application/ThrottleManager.h"

#if WITH_EDITOR
class FVoxelActorBaseEditorTicker : public FVoxelSingleton
{
public:
	//~ Begin FVoxelSingleton Interface
	virtual void Tick() override
	{
		if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
		{
			return;
		}

		// If slate is throttling actor tick (eg, when dragging a float property), force tick

		ForEachObjectOfClass_Copy<AVoxelActorBase>([](AVoxelActorBase& Actor)
		{
			if (Actor.IsTemplate() ||
				!ensure(Actor.GetWorld()))
			{
				return;
			}

			Actor.Tick(FApp::GetDeltaTime());
		});
	}
	//~ End FVoxelSingleton Interface
};
FVoxelActorBaseEditorTicker* GVoxelActorBaseEditorTicker = new FVoxelActorBaseEditorTicker();
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelActorBaseRootComponent::UVoxelActorBaseRootComponent()
{
	bWantsOnUpdateTransform = true;
#if WITH_EDITOR
    bConsiderForActorPlacementWhenHidden = true;
#endif
}

void UVoxelActorBaseRootComponent::UpdateBounds()
{
	VOXEL_FUNCTION_COUNTER();

	Super::UpdateBounds();

	FVoxelTransformRef::NotifyTransformChanged(*this);
}

FBoxSphereBounds UVoxelActorBaseRootComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const FVoxelBox LocalBounds = GetOuterAVoxelActorBase()->GetLocalBounds();
	return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld).ToFBox());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AVoxelActorBase::AVoxelActorBase()
{
	RootComponent = CreateDefaultSubobject<UVoxelActorBaseRootComponent>("RootComponent");

	PrimaryActorTick.bCanEverTick = true;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

AVoxelActorBase::~AVoxelActorBase()
{
	ensure(!PrivateRuntime.IsValid());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActorBase::BeginPlay()
{
	VOXEL_FUNCTION_COUNTER();

	Super::BeginPlay();

	if (bCreateOnBeginPlay &&
		!IsRuntimeCreated())
	{
		QueueCreateRuntime();
	}
}

void AVoxelActorBase::BeginDestroy()
{
	VOXEL_FUNCTION_COUNTER();

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::BeginDestroy();
}

void AVoxelActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	VOXEL_FUNCTION_COUNTER();

	// In the editor, Destroyed is called but EndPlay isn't

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::EndPlay(EndPlayReason);
}

void AVoxelActorBase::Destroyed()
{
	VOXEL_FUNCTION_COUNTER();

	if (IsRuntimeCreated())
	{
		DestroyRuntime();
	}

	Super::Destroyed();
}

void AVoxelActorBase::OnConstruction(const FTransform& Transform)
{
	VOXEL_FUNCTION_COUNTER();

	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (bCreateOnConstruction_EditorOnly &&
		!IsRuntimeCreated() &&
		GetWorld() &&
		!GetWorld()->IsGameWorld() &&
		!IsTemplate() &&
		!IsRunningCommandlet())
	{
		// Force queue to avoid creating runtime twice on map load
		bPrivateCreateQueued = true;
	}
#endif
}

void AVoxelActorBase::PostLoad()
{
	VOXEL_FUNCTION_COUNTER();

	Super::PostLoad();

	FixupProperties();
}

void AVoxelActorBase::PostEditImport()
{
	VOXEL_FUNCTION_COUNTER();

	Super::PostEditImport();

	FixupProperties();

	if (IsRuntimeCreated())
	{
		QueueRecreateRuntime();
	}
}

void AVoxelActorBase::PostInitProperties()
{
	VOXEL_FUNCTION_COUNTER();

	Super::PostInitProperties();

	FixupProperties();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool AVoxelActorBase::Modify(const bool bAlwaysMarkDirty)
{
	if (bPrivateDisableModify)
	{
		return false;
	}

	return Super::Modify(bAlwaysMarkDirty);
}

void AVoxelActorBase::PostEditUndo()
{
	VOXEL_FUNCTION_COUNTER();

	Super::PostEditUndo();

	FixupProperties();

	if (IsValid(this))
	{
		if (!IsRuntimeCreated())
		{
			QueueCreateRuntime();
		}
	}
	else
	{
		if (IsRuntimeCreated())
		{
			DestroyRuntime();
		}
	}
}

void AVoxelActorBase::PreEditChange(FProperty* PropertyThatWillChange)
{
	// Temporarily remove runtime components to avoid expensive re-registration

	for (const TWeakObjectPtr<USceneComponent>& Component : PrivateComponents)
	{
		ensure(Component.IsValid());
		ensure(ConstCast(GetComponents()).Remove(Component.Get()));
	}

	Super::PreEditChange(PropertyThatWillChange);

	for (const TWeakObjectPtr<USceneComponent>& Component : PrivateComponents)
	{
		ensure(Component.IsValid());
		ConstCast(GetComponents()).Add(Component.Get());
	}
}

void AVoxelActorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Temporarily remove runtime components to avoid expensive re-registration

	for (const TWeakObjectPtr<USceneComponent>& Component : PrivateComponents)
	{
		ensure(Component.IsValid());
		ensure(ConstCast(GetComponents()).Remove(Component.Get()));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	for (const TWeakObjectPtr<USceneComponent>& Component : PrivateComponents)
	{
		ensure(Component.IsValid());
		ConstCast(GetComponents()).Add(Component.Get());
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActorBase::Tick(const float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();

	// We don't want to tick the BP in preview
	if (GetWorld()->IsGameWorld())
	{
		Super::Tick(DeltaTime);
	}

	// Check this on Tick to ensure we're in the game thread with a valid transform when initializing
	if (!PrivateTransformRef)
	{
		PrivateTransformRef = MakeSharedCopy(FVoxelTransformRef::Make(*this));
		PrivateTransformRef->AddOnChanged(MakeWeakObjectPtrDelegate(this, [this](const FMatrix&)
		{
			NotifyTransformChanged();
		}));
	}

	if (bPrivateCreateQueued &&
		CanBeCreated())
	{
		CreateRuntime();
	}

	if (ShouldDestroyWhenHidden())
	{
		if (IsHidden() ||
	#if WITH_EDITOR
			IsHiddenEd()
	#else
			false
	#endif
			)
		{
			if (IsRuntimeCreated())
			{
				DestroyRuntime();
				bPrivateCreateOnceVisible = true;
			}
		}
		else if (bPrivateCreateOnceVisible)
		{
			bPrivateCreateOnceVisible = false;
			CreateRuntime();
		}
	}

	FlushRecreateRuntime();

	if (PrivateRuntime)
	{
		PrivateRuntime->Tick();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActorBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	const TSharedPtr<IVoxelActorRuntime> Runtime = CastChecked<AVoxelActorBase>(InThis)->PrivateRuntime;
	if (!Runtime)
	{
		return;
	}

	Runtime->AddReferencedObjects(Collector);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AVoxelActorBase::QueueCreateRuntime()
{
	VOXEL_FUNCTION_COUNTER();

	if (CanBeCreated())
	{
		CreateRuntime();
	}
	else
	{
		bPrivateCreateQueued = true;
	}
}

void AVoxelActorBase::CreateRuntime()
{
	VOXEL_FUNCTION_COUNTER();

	if (!CanBeCreated())
	{
		VOXEL_MESSAGE(Error, "{0}: Cannot create runtime: not ready. See log for more info", this);
		return;
	}

	bPrivateCreateQueued = false;

	if (IsRuntimeCreated())
	{
		return;
	}

	PrivateRuntime = CreateNewRuntime();
	OnRuntimeCreated.Broadcast();

	if (ensure(RootComponent))
	{
		RootComponent->UpdateBounds();
	}
}

void AVoxelActorBase::DestroyRuntime()
{
	VOXEL_FUNCTION_COUNTER();

	// Clear RuntimeRecreate queue
	bPrivateRecreateQueued = false;
	// If the user called this manually we never want to create again
	bPrivateCreateOnceVisible = false;

	if (!IsRuntimeCreated())
	{
		return;
	}

	OnRuntimeDestroyed.Broadcast();

	PrivateRuntime->Destroy();
	PrivateRuntime->bPrivateIsDestroyed = true;
	PrivateRuntime.Reset();

	for (const TWeakObjectPtr<USceneComponent> WeakComponent : PrivateComponents)
	{
		if (USceneComponent* Component = WeakComponent.Get())
		{
			Component->DestroyComponent();
		}
	}
	PrivateComponents.Empty();
	PrivateClassToWeakComponents.Empty();
}

void AVoxelActorBase::FlushRecreateRuntime()
{
	VOXEL_FUNCTION_COUNTER();

	if (bPrivateRecreateQueued &&
		CanBeCreated())
	{
		bPrivateRecreateQueued = false;

		DestroyRuntime();
		CreateRuntime();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USceneComponent* AVoxelActorBase::NewComponent(const UClass* Class)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	ensure(!bPrivateDisableModify);
	bPrivateDisableModify = true;
	ON_SCOPE_EXIT
	{
		ensure(bPrivateDisableModify);
		bPrivateDisableModify = false;
	};

	USceneComponent* Component = nullptr;

	if (TVoxelArray<TWeakObjectPtr<USceneComponent>>* Pool = PrivateClassToWeakComponents.Find(Class))
	{
		while (
			!Component &&
			Pool->Num() > 0)
		{
			Component = Pool->Pop().Get();
		}
	}

	if (!Component)
	{
		if (!ensure(RootComponent))
		{
			return nullptr;
		}

		Component = NewObject<USceneComponent>(
			RootComponent,
			Class,
			NAME_None,
			RF_Transient |
			RF_DuplicateTransient |
			RF_TextExportTransient);

		if (!ensure(Component))
		{
			return nullptr;
		}

		Component->RegisterComponent();
		Component->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

		PrivateComponents.Add(Component);
	}

	Component->SetRelativeTransform(FTransform::Identity);
	return Component;
}

void AVoxelActorBase::RemoveComponent(USceneComponent* Component)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensureVoxelSlow(Component))
	{
		return;
	}

	if (!ensureVoxelSlow(IsRuntimeCreated()))
	{
		Component->DestroyComponent();
		return;
	}

	ensure(PrivateComponents.Contains(Component));
	PrivateClassToWeakComponents.FindOrAdd(Component->GetClass()).Add(Component);
}