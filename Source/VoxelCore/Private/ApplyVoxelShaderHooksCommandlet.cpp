// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "ApplyVoxelShaderHooksCommandlet.h"
#include "VoxelShaderHooksManager.h"

int32 UApplyVoxelShaderHooksCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	for (FVoxelShaderHookGroup* Hook : GVoxelShaderHooksManager->Hooks)
	{
		Hook->Apply();
	}

	bool bFailed = false;
	for (const FVoxelShaderHookGroup* HookGroup : GVoxelShaderHooksManager->Hooks)
	{
		if (HookGroup->GetState() == EVoxelShaderHookState::Active)
		{
			continue;
		}

		bFailed = true;

		FString FailedGUIDs;
		for (const FVoxelShaderHook& Hook : HookGroup->Hooks)
		{
			if (Hook.GetState() == EVoxelShaderHookState::Active)
			{
				continue;
			}

			const FString StateName = INLINE_LAMBDA -> FString
			{
				switch (Hook.GetState())
				{
				case EVoxelShaderHookState::NeverApply: return "Disabled";
				case EVoxelShaderHookState::Active: return "Up to date";
				case EVoxelShaderHookState::Outdated: return "Outdated";
				case EVoxelShaderHookState::NotApplied: return "Not Applied";
				case EVoxelShaderHookState::Invalid: return "Invalid";
				case EVoxelShaderHookState::Deprecated: return "Deprecated";
				default: ensure(false); return "None";
				}
			};

			if (!FailedGUIDs.IsEmpty())
			{
				FailedGUIDs += ", ";
			}

			FailedGUIDs += Hook.ShaderGuid.ToString() + "[" + StateName + "]";
		}

		LOG_VOXEL(Error, "Failed to apply hook %s %s", *HookGroup->DisplayName, *FailedGUIDs);
	}

	if (bFailed)
	{
		return 1;
	}
#else
	check(false);
#endif

	return 0;
}