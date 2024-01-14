// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelMessage.h"
#if WITH_EDITOR
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"

void GatherBlueprintCallstack(const TSharedRef<FVoxelMessage>& Message)
{
	VOXEL_FUNCTION_COUNTER();

	if (!IsInGameThread())
	{
		// No blueprint callstack if we're not in the game thread
		return;
	}

	const TArrayView<const FFrame* const> ScriptStack = FBlueprintContextTracker::Get().GetCurrentScriptStack();

	if (ScriptStack.Num() == 0 ||
		!ensure(ScriptStack.Last()))
	{
		return;
	}
	const FFrame& Frame = *ScriptStack.Last();

	const UClass* Class = FKismetDebugUtilities::FindClassForNode(nullptr, Frame.Node);
	if (!Class)
	{
		return;
	}

	const UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
	if (!Blueprint)
	{
		return;
	}

	const UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Class);
	if (!GeneratedClass ||
		!GeneratedClass->DebugData.IsValid())
	{
		return;
	}

	const int32 CodeOffset = Frame.Code - Frame.Node->Script.GetData() - 1;
	const UEdGraphNode* BlueprintNode = GeneratedClass->DebugData.FindSourceNodeFromCodeLocation(Frame.Node, CodeOffset, true);
	if (!BlueprintNode)
	{
		return;
	}

	Message->AddToken(FVoxelMessageTokenFactory::CreateObjectToken(BlueprintNode));
}

VOXEL_RUN_ON_STARTUP_GAME(RegisterGatherBlueprintCallstack)
{
	GVoxelMessageManager->GatherCallstacks.Add([](const TSharedRef<FVoxelMessage>& Message)
	{
		GatherBlueprintCallstack(Message);
	});
}
#endif