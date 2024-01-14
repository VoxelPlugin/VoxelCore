// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "UObject/GCObject.h"

class FVoxelSingletonManager
	: public FVoxelTicker
	, public FGCObject
{
public:
	FGraphEventRef GraphEvent;
	TVoxelArray<FVoxelSingleton*> Singletons;

	FVoxelSingletonManager() = default;
	virtual ~FVoxelSingletonManager() override
	{
		VOXEL_FUNCTION_COUNTER();

		if (GraphEvent.IsValid())
		{
			VOXEL_SCOPE_COUNTER("Wait");
			GraphEvent->Wait();
		}

		for (const FVoxelSingleton* Singleton : Singletons)
		{
			delete Singleton;
		}
		Singletons.Empty();
	}

public:
	void Tick_Async() const
	{
		VOXEL_FUNCTION_COUNTER();

		for (FVoxelSingleton* Singleton : Singletons)
		{
			Singleton->Tick_Async();
		}
	}

public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		for (FVoxelSingleton* Singleton : Singletons)
		{
			Singleton->Tick();
		}

		if (GraphEvent.IsValid())
		{
			VOXEL_SCOPE_COUNTER("Waiting for Tick_Async");
			GraphEvent->Wait();
		}
		GraphEvent = TGraphTask<TVoxelGraphTask<ENamedThreads::AnyBackgroundThreadNormalTask>>::CreateTask().ConstructAndDispatchWhenReady([this]
		{
			Tick_Async();
		});
	}
	//~ End FVoxelTicker Interface

public:
	//~ Begin FGCObject Interface
	virtual FString GetReferencerName() const override
	{
		return "FVoxelSingletonManager";
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		VOXEL_FUNCTION_COUNTER();

		for (FVoxelSingleton* Singleton : Singletons)
		{
			Singleton->AddReferencedObjects(Collector);
		}
	}
	//~ End FGCObject Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelQueuedSingleton
{
	FVoxelSingleton* Singleton = nullptr;
	FVoxelQueuedSingleton* Next = nullptr;
};
FVoxelQueuedSingleton* GVoxelQueuedCoreSubsystem = nullptr;

FVoxelSingletonManager* GVoxelSingletonManager = nullptr;

void DestroyVoxelSingletonManager()
{
	delete GVoxelSingletonManager;
	GVoxelSingletonManager = nullptr;
}

VOXEL_RUN_ON_STARTUP_GAME(RegisterVoxelSingletonManager )
{
	VOXEL_FUNCTION_COUNTER();

	GVoxelSingletonManager = new FVoxelSingletonManager();

	while (GVoxelQueuedCoreSubsystem)
	{
		const FVoxelQueuedSingleton* QueuedSubsystem = GVoxelQueuedCoreSubsystem;
		GVoxelQueuedCoreSubsystem = QueuedSubsystem->Next;

		if (GIsEditor ||
			!QueuedSubsystem->Singleton->IsEditorOnly())
		{
			QueuedSubsystem->Singleton->Initialize();
			GVoxelSingletonManager->Singletons.Add(QueuedSubsystem->Singleton);
		}

		delete QueuedSubsystem;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSingleton::FVoxelSingleton()
{
	ensure(!GVoxelSingletonManager);

	FVoxelQueuedSingleton* QueuedSubsystem = new FVoxelQueuedSingleton();
	QueuedSubsystem->Singleton = this;
	QueuedSubsystem->Next = GVoxelQueuedCoreSubsystem;
	GVoxelQueuedCoreSubsystem = QueuedSubsystem;
}

FVoxelSingleton::~FVoxelSingleton()
{
	ensure(!GIsRunning);
}