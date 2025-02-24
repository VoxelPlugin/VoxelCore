// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelViewport.h"
#include "AdvancedPreviewScene.h"
#include "SVoxelEditorViewport.h"
#include "VoxelViewportInterface.h"

class FVoxelViewportPreviewScene : public FAdvancedPreviewScene
{
public:
	FVoxelViewportPreviewScene(const ConstructionValues& CVS, const float InFloorOffset = 0.0f)
		: FAdvancedPreviewScene(CVS, InFloorOffset)
	{
	}

	void SetSkyScale(const float Scale) const
	{
		SkyComponent->SetWorldScale3D(FVector(Scale));
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

SVoxelViewport::~SVoxelViewport()
{
	VOXEL_FUNCTION_COUNTER();

	for (AActor* Actor : Actors)
	{
		if (!ensure(Actor))
		{
			continue;
		}

		Actor->Destroy();
	}
}

void SVoxelViewport::Construct(const FArguments& Args)
{
	VOXEL_FUNCTION_COUNTER();

	PreviewScene = MakeShared<FVoxelViewportPreviewScene>(FPreviewScene::ConstructionValues());
	World = PreviewScene->GetWorld();

	INLINE_LAMBDA
	{
		AActor* Actor = SpawnActor<AActor>();
		if (!ensure(Actor))
		{
			return;
		}

		RootComponent = NewObject<USceneComponent>(Actor, NAME_None, RF_Transient);
		if (!ensure(RootComponent))
		{
			return;
		}

		RootComponent->RegisterComponent();
		PreviewScene->AddComponent(RootComponent, FTransform::Identity);
		Actor->SetRootComponent(RootComponent);
	};
}

void SVoxelViewport::Initialize(const TSharedRef<IVoxelViewportInterface>& Interface)
{
	VOXEL_FUNCTION_COUNTER();

	PreviewScene->SetFloorVisibility(Interface->ShowFloor(), true);

	EditorViewport =
		SNew(SVoxelEditorViewport, PreviewScene.ToSharedRef(), Interface)
		.StatsText_Lambda([this]
		{
			return StatsText;
		});

	ChildSlot
	[
		EditorViewport.ToSharedRef()
	];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVector SVoxelViewport::GetViewLocation() const
{
	const TSharedPtr<FEditorViewportClient> Client = EditorViewport->GetViewportClient();
	if (!ensureVoxelSlow(Client))
	{
		return FVector(ForceInit);
	}

	return Client->GetViewLocation();
}

FRotator SVoxelViewport::GetViewRotation() const
{
	const TSharedPtr<FEditorViewportClient> Client = EditorViewport->GetViewportClient();
	if (!ensureVoxelSlow(Client))
	{
		return FRotator(ForceInit);
	}

	return Client->GetViewRotation();
}

void SVoxelViewport::SetViewLocation(const FVector& Location)
{
	const TSharedPtr<FEditorViewportClient> Client = EditorViewport->GetViewportClient();
	if (!ensureVoxelSlow(Client))
	{
		return;
	}

	Client->SetViewLocation(Location);
}

void SVoxelViewport::SetViewRotation(const FRotator& Rotation)
{
	const TSharedPtr<FEditorViewportClient> Client = EditorViewport->GetViewportClient();
	if (!ensureVoxelSlow(Client))
	{
		return;
	}

	Client->SetViewRotation(Rotation);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelViewport::SetStatsText(const FString& Text)
{
	StatsText = FText::FromString(Text);
}

void SVoxelViewport::SetFloorScale(const FVector& Scale)
{
	ConstCast(PreviewScene->GetFloorMeshComponent())->SetWorldScale3D(Scale);
}

void SVoxelViewport::SetSkyScale(const float Scale)
{
	PreviewScene->SetSkyScale(Scale);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AActor* SVoxelViewport::SpawnActor(UClass* Class)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(World))
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.bNoFail = true;
	Parameters.ObjectFlags = RF_Transient;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.Name = MakeUniqueObjectName(World, Class, FName(Class->GetName() + "_PreviewScene"));

	AActor* Actor = World->SpawnActor(Class, nullptr, nullptr, Parameters);
	if (!ensure(Actor))
	{
		return nullptr;
	}

	Actors.Add(Actor);
	return Actor;
}

UActorComponent* SVoxelViewport::CreateComponent(UClass* Class)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(RootComponent))
	{
		return nullptr;
	}

	AActor* Actor = RootComponent->GetOwner();
	if (!ensure(Actor))
	{
		return nullptr;
	}

	UActorComponent* Component = NewObject<UActorComponent>(Actor, Class, NAME_None, RF_Transient);
	if (!ensure(Component))
	{
		return nullptr;
	}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		SceneComponent->SetupAttachment(RootComponent);
		SceneComponent->SetWorldTransform(FTransform::Identity);
	}

	Component->RegisterComponent();
	return Component;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString SVoxelViewport::GetReferencerName() const
{
	return "SVoxelViewport";
}

void SVoxelViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	Collector.AddReferencedObject(World);
	Collector.AddReferencedObject(RootComponent);
	Collector.AddReferencedObjects(Actors);
}