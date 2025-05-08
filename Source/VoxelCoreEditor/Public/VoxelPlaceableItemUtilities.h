// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class VOXELCOREEDITOR_API FVoxelPlaceableItemUtilities
{
public:
	static void RegisterActorFactory(const UClass* ActorFactoryClass);
};

#define DEFINE_VOXEL_PLACEABLE_ITEM_FACTORY(ActorFactory) \
	VOXEL_RUN_ON_STARTUP_EDITOR() \
	{ \
		FVoxelPlaceableItemUtilities::RegisterActorFactory(ActorFactory::StaticClass()); \
	}