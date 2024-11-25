// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelCoreCommands.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.RefreshAll",
	"")
{
	Voxel::RefreshAll();
}

FSimpleMulticastDelegate GVoxelOnRefreshAll;

void Voxel::RefreshAll()
{
	VOXEL_FUNCTION_COUNTER();

	GVoxelOnRefreshAll.Broadcast();
}