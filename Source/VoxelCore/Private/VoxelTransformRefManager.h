// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelTransformRefImpl.h"

class FVoxelTransformRefManager : public FVoxelSingleton
{
public:
	TSharedRef<FVoxelTransformRefImpl> Make_AnyThread(TConstVoxelArrayView<FVoxelTransformRefNode> Nodes);
	TSharedPtr<FVoxelTransformRefImpl> Find_AnyThread_RequiresLock(const FVoxelTransformRefNodeArray& NodeArray) const;

	void NotifyTransformChanged(const USceneComponent& Component);

	//~ Begin FVoxelSingleton Interface
	virtual void Tick() override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FVoxelSingleton Interface

private:
	double LastClearTime = 0;

	FVoxelCriticalSection CriticalSection;
	TVoxelArray<TSharedPtr<FVoxelTransformRefImpl>> SharedTransformRefs_RequiresLock;
	TVoxelMap<FObjectKey, TVoxelSet<TWeakPtr<FVoxelTransformRefImpl>>> ComponentToWeakTransformRefs_RequiresLock;
	TVoxelMap<FVoxelTransformRefNodeArray, TWeakPtr<FVoxelTransformRefImpl>> NodeArrayToWeakTransformRef_RequiresLock;
};
extern FVoxelTransformRefManager* GVoxelTransformRefManager;