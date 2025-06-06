// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.RefreshAll",
	"Refresh everything")
{
	Voxel::RefreshAll();
}

FSimpleMulticastDelegate Voxel::OnRefreshAll;

void Voxel::RefreshAll()
{
	VOXEL_FUNCTION_COUNTER();

	LOG_VOXEL(Log, "Voxel::RefreshAll");

	Voxel::OnRefreshAll.Broadcast();
}