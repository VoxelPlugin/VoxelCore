// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelCoreCommands.h"
#include "LevelEditor.h"

class FVoxelCoreCommands : public TVoxelCommands<FVoxelCoreCommands>
{
public:
	TSharedPtr<FUICommandInfo> RefreshAll;

	virtual void RegisterCommands() override;
};

DEFINE_VOXEL_COMMANDS(FVoxelCoreCommands);

void FVoxelCoreCommands::RegisterCommands()
{
	VOXEL_UI_COMMAND(RefreshAll, "Refresh", "Refresh all voxel actors", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F5));
}

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	FVoxelCoreCommands::Register();

	const FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	FUICommandList& Actions = *LevelEditorModule.GetGlobalLevelEditorActions();

	Actions.MapAction(FVoxelCoreCommands::Get().RefreshAll, MakeLambdaDelegate([]
	{
		Voxel::RefreshAll();
	}));
}