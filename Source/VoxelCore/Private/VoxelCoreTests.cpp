// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

VOXEL_RUN_ON_STARTUP_GAME()
{
	if (!FVoxelUtilities::IsDevWorkflow())
	{
		return;
	}

	{
		TVoxelMap<int32, TSharedPtr<int32>> Map;

		TSharedPtr<int32> Shared = MakeShared<int32>();
		Map.Add_EnsureNew(0, Shared);
		check(Shared);
		Map.Add_EnsureNew(1, MoveTemp(Shared));
		check(!Shared);
	}

	{
		TVoxelSet<int32> Set;
		TVoxelSet<int32> Set2 = Set;
		TVoxelSet<float> Set3 = TVoxelSet<float>(Set);
	}
}