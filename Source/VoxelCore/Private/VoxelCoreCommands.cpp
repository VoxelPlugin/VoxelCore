// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelCoreCommands.h"
#include "VoxelActorBase.h"
#include "VoxelCoreSettings.h"
#include "VoxelMovingAverageBuffer.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#endif

VOXEL_CONSOLE_COMMAND(
	"voxel.RefreshAll",
	"")
{
	Voxel::RefreshAll();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSimpleMulticastDelegate GVoxelOnRefreshAll;
TSet<FObjectKey> GVoxelObjectsDestroyedByFrameRateLimit;

void Voxel::RefreshAll()
{
	VOXEL_FUNCTION_COUNTER();

	GVoxelOnRefreshAll.Broadcast();

	ForEachObjectOfClass<AVoxelActorBase>([&](AVoxelActorBase& Actor)
	{
		if (Actor.IsRuntimeCreated() ||
			GVoxelObjectsDestroyedByFrameRateLimit.Contains(&Actor))
		{
			Actor.QueueRecreateRuntime();
		}
	});

	GVoxelObjectsDestroyedByFrameRateLimit.Empty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
class FVoxelSafetyTicker : public FVoxelEditorSingleton
{
public:
	//~ Begin FVoxelEditorSingleton Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		if (!GetDefault<UVoxelCoreSettings>()->bEnablePerformanceMonitoring)
		{
			return;
		}

		if (GEditor->ShouldThrottleCPUUsage() ||
			GEditor->PlayWorld ||
			GIsPlayInEditorWorld)
		{
			// Don't check framerate when throttling or in PIE
			return;
		}

		const int32 FramesToAverage = FMath::Max(2, GetDefault<UVoxelCoreSettings>()->FramesToAverage);
		if (FramesToAverage != Buffer.GetWindowSize())
		{
			Buffer = FVoxelMovingAverageBuffer(FramesToAverage);
		}

		// Avoid outliers (typically, debugger breaking) causing a huge average
		const double SanitizedDeltaTime = FMath::Clamp(FApp::GetDeltaTime(), 0.001, 1);
		Buffer.AddValue(SanitizedDeltaTime);

		if (1.f / Buffer.GetAverageValue() > GetDefault<UVoxelCoreSettings>()->MinFPS)
		{
			bDestroyedRuntimes = false;
			return;
		}

		if (bDestroyedRuntimes)
		{
			return;
		}
		bDestroyedRuntimes = true;

		FNotificationInfo Info(INVTEXT("Average framerate is below 8fps, destroying all voxel runtimes. Use Ctrl F5 to re-create them"));
		Info.ExpireDuration = 4.f;
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			INVTEXT("Disable Monitoring"),
			INVTEXT("Disable framerate monitoring"),
			MakeLambdaDelegate([]
			{
				GetMutableDefault<UVoxelCoreSettings>()->bEnablePerformanceMonitoring = false;
				GetMutableDefault<UVoxelCoreSettings>()->PostEditChange();

				GEngine->Exec(nullptr, TEXT("voxel.RefreshAll"));
			}),
			SNotificationItem::CS_None));
		FSlateNotificationManager::Get().AddNotification(Info);

		ForEachObjectOfClass_Copy<AVoxelActorBase>([&](AVoxelActorBase& Actor)
		{
			if (!Actor.IsRuntimeCreated())
			{
				return;
			}

			if (!ensure(Actor.GetWorld()) ||
				!Actor.GetWorld()->IsEditorWorld())
			{
				return;
			}

			Actor.DestroyRuntime();
			GVoxelObjectsDestroyedByFrameRateLimit.Add(&Actor);
		});
	}
	//~ End FVoxelEditorSingleton Interface

private:
	FVoxelMovingAverageBuffer Buffer = FVoxelMovingAverageBuffer(2);
	bool bDestroyedRuntimes = false;
};
FVoxelSafetyTicker* GVoxelSafetyTicker = new FVoxelSafetyTicker();
#endif