// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

DEFINE_VOXEL_INSTANCE_COUNTER(IVoxelWorldSubsystem);

FORCEINLINE const TCHAR* LexToString(const ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Client: return TEXT("Client");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer: return TEXT("ListenServer");
	case NM_Standalone: return TEXT("Standalone");
	default: return TEXT("Invalid");
	}
}

class FVoxelWorldSubsystemManager : public FVoxelSingleton
{
public:
	FVoxelCriticalSection CriticalSection;
	TVoxelMap<TVoxelObjectPtr<const UWorld>, TVoxelMap<FName, TSharedPtr<IVoxelWorldSubsystem>>> WorldToNameToSubsystem_RequiresLock;

	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		GOnVoxelModuleUnloaded_DoCleanup.AddLambda([this]
		{
			VOXEL_SCOPE_LOCK(CriticalSection);
			WorldToNameToSubsystem_RequiresLock.Empty();
		});
	}
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		TVoxelArray<TPair<const UWorld*, TVoxelArray<TSharedPtr<IVoxelWorldSubsystem>>>> WorldToSubsystems;
		{
			VOXEL_SCOPE_LOCK(CriticalSection);

			WorldToSubsystems.Reserve(WorldToNameToSubsystem_RequiresLock.Num());

			for (auto It = WorldToNameToSubsystem_RequiresLock.CreateIterator(); It; ++It)
			{
				const UWorld* World = It.Key().Resolve();
				if (!World)
				{
					It.RemoveCurrent();
					continue;
				}

				TVoxelArray<TSharedPtr<IVoxelWorldSubsystem>> Subsystems;
				Subsystems.Reserve(It.Value().Num());
				for (const auto& SubsystemIt : It.Value())
				{
					Subsystems.Add(SubsystemIt.Value);
				}

				WorldToSubsystems.Add({ World, MoveTemp(Subsystems) });
			}
		}

		for (const auto& WorldIt : WorldToSubsystems)
		{
			VOXEL_SCOPE_COUNTER_FORMAT("%s %s",
				*WorldIt.Key->GetPathName(),
				LexToString(WorldIt.Key->GetNetMode()));

			for (const TSharedPtr<IVoxelWorldSubsystem>& Subsystem : WorldIt.Value)
			{
				Subsystem->Tick();
			}
		}
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		VOXEL_FUNCTION_COUNTER();
		VOXEL_SCOPE_LOCK(CriticalSection);

		for (const auto& WorldIt : WorldToNameToSubsystem_RequiresLock)
		{
			for (const auto& NameIt : WorldIt.Value)
			{
				NameIt.Value->AddReferencedObjects(Collector);
			}
		}
	}
	//~ End FVoxelSingleton Interface
};
FVoxelWorldSubsystemManager* GVoxelWorldSubsystemManager = new FVoxelWorldSubsystemManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<IVoxelWorldSubsystem> IVoxelWorldSubsystem::GetInternal(
	const TVoxelObjectPtr<const UWorld> World,
	const FName Name,
	TSharedRef<IVoxelWorldSubsystem>(*Constructor)())
{
	ensureVoxelSlow(!World.IsExplicitlyNull());
	ensureVoxelSlowNoSideEffects(!IsInGameThread() || World.IsValid_Slow());
	VOXEL_SCOPE_LOCK(GVoxelWorldSubsystemManager->CriticalSection);

	TSharedPtr<IVoxelWorldSubsystem>& Subsystem = GVoxelWorldSubsystemManager->WorldToNameToSubsystem_RequiresLock.FindOrAdd(World).FindOrAdd(Name);
	if (!Subsystem)
	{
		Subsystem = (*Constructor)();
		Subsystem->PrivateWorld = ConstCast(World);
	}
	return Subsystem.ToSharedRef();
}

TVoxelArray<TSharedRef<IVoxelWorldSubsystem>> IVoxelWorldSubsystem::GetAllInternal(const FName Name)
{
	VOXEL_SCOPE_LOCK(GVoxelWorldSubsystemManager->CriticalSection);

	TVoxelArray<TSharedRef<IVoxelWorldSubsystem>> Subsystems;
	for (const auto& It : GVoxelWorldSubsystemManager->WorldToNameToSubsystem_RequiresLock)
	{
		if (const TSharedPtr<IVoxelWorldSubsystem> Subsystem = It.Value.FindRef(Name))
		{
			Subsystems.Add(Subsystem.ToSharedRef());
		}
	}
	return Subsystems;
}