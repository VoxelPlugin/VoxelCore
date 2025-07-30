// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelShaderHooksManager.h"

#if WITH_EDITOR
FVoxelShaderHooksManager* GVoxelShaderHooksManager = new FVoxelShaderHooksManager();

void FVoxelShaderHooksManager::RegisterHook(FVoxelShaderHookGroup& Group)
{
	check(IsInGameThread());

	Hooks.Add(&Group);

	for (const FVoxelShaderHook& Hook : Group.Hooks)
	{
		check(!GuidToHook.Contains(Hook.ShaderGuid));
		GuidToHook.Add_CheckNew(Hook.ShaderGuid, &Hook);
	}
}
#endif