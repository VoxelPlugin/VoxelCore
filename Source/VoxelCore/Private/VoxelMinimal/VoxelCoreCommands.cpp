// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.RefreshAll",
	"")
{
	Voxel::RefreshAll();
}

FSimpleMulticastDelegate Voxel::OnRefreshAll;

void Voxel::RefreshAll()
{
	VOXEL_FUNCTION_COUNTER();

	Voxel::OnRefreshAll.Broadcast();
}