// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelSingletonManager.h"

FVoxelSingleton::FVoxelSingleton()
{
	FVoxelSingletonManager::RegisterSingleton(*this);
}

FVoxelSingleton::~FVoxelSingleton()
{
	ensure(!GIsRunning);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelRenderSingleton::FVoxelRenderSingleton()
{
	bIsRenderSingleton = true;
}