// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSingletonManager.h"
#include "VoxelSingletonSceneViewExtension.h"

struct FVoxelQueuedSingleton
{
	FVoxelSingleton* Singleton = nullptr;
	FVoxelQueuedSingleton* Next = nullptr;
};
FVoxelQueuedSingleton* GVoxelQueuedSingleton = nullptr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSingletonManager* GVoxelSingletonManager = nullptr;

VOXEL_RUN_ON_STARTUP_GAME()
{
	// Do this once all singletons have been queued
	GVoxelSingletonManager = new FVoxelSingletonManager();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSingletonManager::RegisterSingleton(FVoxelSingleton& Singleton)
{
	ensure(!GVoxelSingletonManager);

	FVoxelQueuedSingleton* QueuedSingleton = new FVoxelQueuedSingleton();
	QueuedSingleton->Singleton = &Singleton;
	QueuedSingleton->Next = GVoxelQueuedSingleton;
	GVoxelQueuedSingleton = QueuedSingleton;
}

void FVoxelSingletonManager::Destroy()
{
	ensure(GVoxelSingletonManager);

	delete GVoxelSingletonManager;
	GVoxelSingletonManager = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSingletonManager::FVoxelSingletonManager()
{
	VOXEL_FUNCTION_COUNTER();

	while (GVoxelQueuedSingleton)
	{
		FVoxelSingleton* Singleton = GVoxelQueuedSingleton->Singleton;

		{
			FVoxelQueuedSingleton* Next = GVoxelQueuedSingleton->Next;
			delete GVoxelQueuedSingleton;
			GVoxelQueuedSingleton = Next;
		}

		if (!GIsEditor &&
			Singleton->IsEditorOnly())
		{
			// Editor singletons should be within WITH_EDITOR
			ensure(WITH_EDITOR);
			continue;
		}

		Singletons.Add(Singleton);
	}
	check(!GVoxelQueuedSingleton);

	for (FVoxelSingleton* Singleton : Singletons)
	{
		check(!Singleton->bIsInitialized);
		{
			Singleton->Initialize();
		}
		check(!Singleton->bIsInitialized);
		Singleton->bIsInitialized = true;
	}

	FVoxelUtilities::DelayedCall([this]
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FVoxelSingletonSceneViewExtension>();

		const TSharedRef<FSharedVoidPtr> SharedSharedRef = MakeSharedCopy(MakeSharedVoid().ToSharedPtr());

		// Ensure BeginFrame is called as many times as EndFrame
		FCoreDelegates::OnEndFrameRT.Add(MakeWeakPtrDelegate(*SharedSharedRef, [this, SharedSharedRef]
		{
			SharedSharedRef->Reset();

			FCoreDelegates::OnBeginFrameRT.AddLambda([this]
			{
				ViewExtension->OnBeginFrame_RenderThread();
			});
			FCoreDelegates::OnEndFrameRT.AddLambda([this]
			{
				ViewExtension->OnEndFrame_RenderThread();
			});
		}));

		for (FVoxelSingleton* Singleton : Singletons)
		{
			if (!Singleton->bIsRenderSingleton)
			{
				continue;
			}

			ViewExtension->Singletons.Add(static_cast<FVoxelRenderSingleton*>(Singleton));
		}
	});
}

FVoxelSingletonManager::~FVoxelSingletonManager()
{
	VOXEL_FUNCTION_COUNTER();

	for (const FVoxelSingleton* Singleton : Singletons)
	{
		delete Singleton;
	}
	Singletons.Empty();

	if (ViewExtension)
	{
		ViewExtension->Singletons.Empty();
		ViewExtension.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSingletonManager::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelSingleton* Singleton : Singletons)
	{
		Singleton->Tick();
	}

	Voxel::RenderTask([this](FRHICommandList& RHICmdList)
	{
		VOXEL_SCOPE_COUNTER("FVoxelSingletonManager::Tick_RenderThread");
		check(IsInRenderingThread());

		for (FVoxelSingleton* Singleton : Singletons)
		{
			Singleton->Tick_RenderThread(RHICmdList);
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelSingletonManager::GetReferencerName() const
{
	return "FVoxelSingletonManager";
}

void FVoxelSingletonManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	for (FVoxelSingleton* Singleton : Singletons)
	{
		Singleton->AddReferencedObjects(Collector);
	}
}
