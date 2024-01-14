// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "LevelEditor.h"

class FVoxelEditorCommands : public TVoxelCommands<FVoxelEditorCommands>
{
public:
	TSharedPtr<FUICommandInfo> RefreshAll;

	virtual void RegisterCommands() override;
};

DEFINE_VOXEL_COMMANDS(FVoxelEditorCommands);

void FVoxelEditorCommands::RegisterCommands()
{
	VOXEL_UI_COMMAND(RefreshAll, "Refresh", "Refresh all voxel graphs", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F5));
}

VOXEL_RUN_ON_STARTUP_EDITOR(RegisterVoxelEditorCommands)
{
	FVoxelEditorCommands::Register();

	const FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	FUICommandList& Actions = *LevelEditorModule.GetGlobalLevelEditorActions();

	Actions.MapAction(FVoxelEditorCommands::Get().RefreshAll, MakeLambdaDelegate([]
	{
		GEngine->Exec(nullptr, TEXT("voxel.RefreshAll"));
	}));
}