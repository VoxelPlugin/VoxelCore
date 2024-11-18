// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelSingletonSceneViewExtension;

class VOXELCORE_API FVoxelSingletonManager
	: public FVoxelTicker
	, public FGCObject
{
public:
	static void RegisterSingleton(FVoxelSingleton& Singleton);
	static void Destroy();

public:
	FVoxelSingletonManager();
	virtual ~FVoxelSingletonManager() override;

public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override;
	//~ End FVoxelTicker Interface

public:
	//~ Begin FGCObject Interface
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

private:
	FGraphEventRef GraphEvent;
	TVoxelArray<FVoxelSingleton*> Singletons;
	TSharedPtr<FVoxelSingletonSceneViewExtension> ViewExtension;
};
extern VOXELCORE_API FVoxelSingletonManager* GVoxelSingletonManager;