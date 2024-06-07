// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"

#if WITH_EDITOR
#include "Selection.h"
#include "EditorViewportClient.h"
#endif

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelFreezeCamera, false,
	"voxel.FreezeCamera",
	"");

FViewport* FVoxelUtilities::GetViewport(const UWorld* World)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (!World)
	{
		return nullptr;
	}

	if (World->IsGameWorld())
	{
		const UGameViewportClient* Viewport = World->GetGameViewport();
		if (!Viewport)
		{
			return nullptr;
		}
		return Viewport->Viewport;
	}
	else
	{
#if WITH_EDITOR
		TVoxelInlineArray<const FEditorViewportClient*, 8> ValidClients;
		for (const FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
		{
			if (ViewportClient->GetWorld() == World)
			{
				ValidClients.Add(ViewportClient);
			}
		}

		const FViewport* ActiveViewport = GEditor->GetActiveViewport();
		for (const FEditorViewportClient* ViewportClient : ValidClients)
		{
			if (ViewportClient->Viewport == ActiveViewport)
			{
				return ViewportClient->Viewport;
			}
		}

		if (ValidClients.Num() > 0)
		{
			return ValidClients.Last()->Viewport;
		}
#endif

		return nullptr;
	}
}

bool FVoxelUtilities::GetCameraView(const UWorld* World, FVector& OutPosition, FRotator& OutRotation, float& OutFOV)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (!World)
	{
		return false;
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		// Never allow accessing the camera position on servers
		// It would incorrectly return the camera position of the first player
		return false;
	}

	const bool bSuccess = INLINE_LAMBDA
	{
		if (World->IsGameWorld())
		{
			const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(World, 0);
			if (!CameraManager)
			{
				return false;
			}

			OutPosition = CameraManager->GetCameraLocation();
			OutRotation = CameraManager->GetCameraRotation();
			OutFOV = CameraManager->GetFOVAngle();

			return true;
		}
		else
		{
	#if WITH_EDITOR
			const FEditorViewportClient* BestViewportClient = INLINE_LAMBDA -> const FEditorViewportClient*
			{
				TVoxelInlineArray<const FEditorViewportClient*, 8> ValidClients;
				for (const FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
				{
					if (ViewportClient->GetWorld() == World)
					{
						ValidClients.Add(ViewportClient);
					}
				}

				const FViewport* ActiveViewport = GEditor->GetActiveViewport();
				const FEditorViewportClient* ActiveViewportClient = nullptr;
				for (const FEditorViewportClient* ViewportClient : ValidClients)
				{
					if (ViewportClient->Viewport == ActiveViewport)
					{
						ActiveViewportClient =  ViewportClient;
					}
				}

				if (ActiveViewportClient)
				{
					// Try to find a perspective client if we have one
					if (!ActiveViewportClient->IsPerspective())
					{
						for (const FEditorViewportClient* ViewportClient : ValidClients)
						{
							if (ViewportClient->IsPerspective())
							{
								return ViewportClient;
							}
						}
					}

					return ActiveViewportClient;
				}

				if (ValidClients.Num() > 0)
				{
					return ValidClients.Last();
				}

				return nullptr;
			};

			if (!BestViewportClient)
			{
				return false;
			}

			OutPosition = BestViewportClient->GetViewLocation();
			OutRotation = BestViewportClient->GetViewRotation();
			OutFOV = BestViewportClient->FOVAngle;

			return true;
	#endif

			return false;
		}
	};

	if (!bSuccess)
	{
		return false;
	}

	struct FCacheEntry
	{
		FVector Position;
		FRotator Rotation;
		float FOV;
	};
	static TMap<FObjectKey, FCacheEntry> Cache;

	if (GVoxelFreezeCamera)
	{
		if (const FCacheEntry* CacheEntry = Cache.Find(World))
		{
			OutPosition = CacheEntry->Position;
			OutRotation = CacheEntry->Rotation;
			OutFOV = CacheEntry->FOV;
			return true;
		}
	}

	Cache.FindOrAdd(World) = { OutPosition, OutRotation, OutFOV };
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
class FVoxelActorSelectionTracker : public FVoxelSingleton
{
public:
	TVoxelMap<FObjectKey, TMulticastDelegate<void(bool)>> ActorToDelegate;

	FVoxelCriticalSection CriticalSection;
	TVoxelSet<FObjectKey> SelectedActors_RequiresLock;

	double LastCleanup = FPlatformTime::Seconds();

	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		USelection::SelectionChangedEvent.AddLambda([this](UObject*)
		{
			UpdateSelection();
		});
	}
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		if (FPlatformTime::Seconds() - LastCleanup < 30.f)
		{
			return;
		}
		LastCleanup = FPlatformTime::Seconds();

		for (auto It = ActorToDelegate.CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}
	//~ End FVoxelSingleton Interface

public:
	void UpdateSelection()
	{
		VOXEL_FUNCTION_COUNTER();

		TVoxelSet<FObjectKey> NewSelectedActors;
		{
			USelection* Selection = GEditor->GetSelectedActors();
			NewSelectedActors.Reserve(Selection->Num());

			for (FSelectionIterator It(*Selection); It; ++It)
			{
				const AActor* Actor = CastChecked<AActor>(*It);
				NewSelectedActors.Add(Actor);
			}
		}

		TVoxelSet<FObjectKey> ActorsToSelect;
		TVoxelSet<FObjectKey> ActorsToDeselect;
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			ActorsToSelect = NewSelectedActors.Difference(SelectedActors_RequiresLock);
			ActorsToDeselect = SelectedActors_RequiresLock.Difference(NewSelectedActors);

			SelectedActors_RequiresLock = MoveTemp(NewSelectedActors);
		}

		for (const FObjectKey& Actor : ActorsToSelect)
		{
			const TMulticastDelegate<void(bool)>* Delegate = ActorToDelegate.Find(Actor);
			if (!Delegate)
			{
				continue;
			}

			Delegate->Broadcast(true);
		}

		for (const FObjectKey& Actor : ActorsToDeselect)
		{
			const TMulticastDelegate<void(bool)>* Delegate = ActorToDelegate.Find(Actor);
			if (!Delegate)
			{
				continue;
			}

			Delegate->Broadcast(false);
		}
	}
};
FVoxelActorSelectionTracker* GVoxelActorSelectionTracker = new FVoxelActorSelectionTracker();

void FVoxelUtilities::OnActorSelectionChanged(
	AActor& Actor,
	TDelegate<void(bool bIsSelected)> Delegate)
{
	GVoxelActorSelectionTracker->ActorToDelegate.FindOrAdd(&Actor).Add(MoveTemp(Delegate));
}

bool FVoxelUtilities::IsActorSelected_AnyThread(const FObjectKey Actor)
{
	VOXEL_SCOPE_LOCK(GVoxelActorSelectionTracker->CriticalSection);
	return GVoxelActorSelectionTracker->SelectedActors_RequiresLock.Contains(Actor);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::CopyBodyInstance(
	FBodyInstance& Dest,
	const FBodyInstance& Source)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	Dest.CopyRuntimeBodyInstancePropertiesFrom(&Source);
	Dest.SetObjectType(Source.GetObjectType());
}

bool FVoxelUtilities::BodyInstanceEqual(
	const FBodyInstance& A,
	const FBodyInstance& B)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (A.GetObjectType() != B.GetObjectType() ||
		A.GetOverrideWalkableSlopeOnInstance() != B.GetOverrideWalkableSlopeOnInstance() ||
		A.GetWalkableSlopeOverride().GetWalkableSlopeBehavior() != B.GetWalkableSlopeOverride().GetWalkableSlopeBehavior() ||
		A.GetWalkableSlopeOverride().GetWalkableSlopeAngle() != B.GetWalkableSlopeOverride().GetWalkableSlopeAngle() ||
		A.GetResponseToChannels() != B.GetResponseToChannels() ||
		A.GetCollisionProfileName() != B.GetCollisionProfileName() ||
		A.GetCollisionEnabled() != B.GetCollisionEnabled())
	{
		return false;
	}

	return true;
}