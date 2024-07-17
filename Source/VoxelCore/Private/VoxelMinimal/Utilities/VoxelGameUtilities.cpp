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

TOptional<FVector> FVoxelUtilities::GetCameraPosition(const UWorld* World)
{
	FVector Position;
	FRotator Rotation;
	float FOV;
	if (!GetCameraView(World, Position, Rotation, FOV))
	{
		return {};
	}

	return Position;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelUtilities::ComputeInvokerChunks(
	TVoxelSet<FIntVector>& OutChunks,
	TVoxelArray<FSphere> Invokers,
	const FMatrix& LocalToWorld,
	const double ChunkSize,
	const int32 MaxNumChunks)
{
	VOXEL_FUNCTION_COUNTER_NUM(Invokers.Num(), 1);

	Invokers.Sort([](const FSphere& A, const FSphere& B)
	{
		return A.W > B.W;
	});

	{
		VOXEL_SCOPE_COUNTER("Remove duplicates");

		for (int32 IndexA = 0; IndexA < Invokers.Num(); IndexA++)
		{
			const FSphere InvokerA = Invokers[IndexA];
			for (int32 IndexB = 0; IndexB < IndexA; IndexB++)
			{
				const FSphere InvokerB = Invokers[IndexB];

				const double RadiusDelta = InvokerB.W - InvokerA.W;
				ensureVoxelSlow(RadiusDelta >= 0);

				if (FVector::DistSquared(InvokerA.Center, InvokerB.Center) > FMath::Square(RadiusDelta))
				{
					continue;
				}

				// If distance between two centers is less than the difference of radius, then A is fully contained in B
				Invokers.RemoveAtSwap(IndexA);
				IndexA--;
				break;
			}
		}
	}

	struct FChunkedInvoker
	{
		FVector Center;
		double RadiusInChunks;
	};
	TVoxelArray<FChunkedInvoker> ChunkedInvokers;
	ChunkedInvokers.Reserve(Invokers.Num());
	{
		VOXEL_SCOPE_COUNTER("Make ChunkedInvokers");

		const FMatrix WorldToLocal = LocalToWorld.Inverse();
		const float WorldToLocalScale = WorldToLocal.GetMaximumAxisScale();

		for (const FSphere& Invoker : Invokers)
		{
			const FVector LocalPosition = WorldToLocal.TransformPosition(Invoker.Center);
			const double LocalRadius = Invoker.W * WorldToLocalScale;

			FChunkedInvoker ChunkedInvoker;
			ChunkedInvoker.Center = LocalPosition / ChunkSize;
			ChunkedInvoker.RadiusInChunks = LocalRadius / ChunkSize;
			ChunkedInvokers.Add(ChunkedInvoker);
		}
	}

	OutChunks.Reset();

	{
		double Num = 0;
		for (const FChunkedInvoker& Invoker : ChunkedInvokers)
		{
			Num += FMath::Cube(2 * Invoker.RadiusInChunks + 1);
		}
		OutChunks.Reserve(FMath::Min<int64>(FMath::CeilToInt64(Num), 32768));
	}

	for (const FChunkedInvoker& Invoker : ChunkedInvokers)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Add invoker Radius=%f chunks", Invoker.RadiusInChunks);

		// Offset due to chunk position being the chunk lower corner
		constexpr double ChunkOffset = 0.5;
		// We want to check the chunk against invoker, not the chunk center
		// To avoid a somewhat expensive box-to-point distance, we offset the invoker radius by the chunk diagonal
		// (from chunk center to any chunk corner)
		constexpr double ChunkHalfDiagonal = UE_SQRT_3 / 2.;

		const FIntVector Min = FloorToInt(Invoker.Center - Invoker.RadiusInChunks - ChunkOffset);
		const FIntVector Max = CeilToInt(Invoker.Center + Invoker.RadiusInChunks - ChunkOffset);
		const double RadiusSquared = FMath::Square(Invoker.RadiusInChunks + ChunkHalfDiagonal);

		for (int32 X = Min.X; X <= Max.X; X++)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; Y++)
			{
				for (int32 Z = Min.Z; Z <= Max.Z; Z++)
				{
					const double DistanceSquared = (FVector(X, Y, Z) + ChunkOffset - Invoker.Center).SizeSquared();
					if (DistanceSquared > RadiusSquared)
					{
						continue;
					}

					OutChunks.Add(FIntVector(X, Y, Z));
				}

				if (OutChunks.Num() > MaxNumChunks)
				{
					return false;
				}
			}
		}
	}

	OutChunks.Shrink();

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