// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelMessage.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "VoxelMessageToken_BlueprintCallstack.h"

DEFINE_PRIVATE_ACCESS(FBlueprintDebugData, PerFunctionLineNumbers);

void GatherBlueprintCallstack(const TSharedRef<FVoxelMessage>& Message)
{
	VOXEL_FUNCTION_COUNTER();

	if (!IsInGameThread())
	{
		// No blueprint callstack if we're not in the game thread
		return;
	}

	const TVoxelArrayView<const FFrame* const> ScriptStack = FBlueprintContextTracker::Get().GetCurrentScriptStack();

	if (ScriptStack.Num() == 0 ||
		!ensure(ScriptStack.Last()))
	{
		return;
	}

	TArray<TWeakObjectPtr<const UEdGraphNode>> Callstack;
	TSet<const UEdGraphNode*> AddedNodes;
	for (const FFrame* Frame : ScriptStack)
	{
		if (!Frame)
		{
			continue;
		}

		const UClass* Class = FKismetDebugUtilities::FindClassForNode(nullptr, Frame->Node);
		if (!Class)
		{
			continue;
		}

		const UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
		if (!Blueprint)
		{
			continue;
		}

		const UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Class);
		if (!GeneratedClass ||
			!GeneratedClass->DebugData.IsValid())
		{
			continue;
		}

		const int32 CodeOffset = Frame->Code - Frame->Node->Script.GetData() - 1;

		const FDebuggingInfoForSingleFunction* FunctionInfo = PrivateAccess::PerFunctionLineNumbers(GeneratedClass->DebugData).Find(Frame->Node);
		if (!FunctionInfo)
		{
			continue;
		}

		TVoxelArray<int32> CodeOffsets;
		FunctionInfo->LineNumberToSourceNodeMap.GenerateKeyArray(CodeOffsets);
		CodeOffsets.Sort();

		if (!ensureVoxelSlow(CodeOffsets.Num() > 0))
		{
			continue;
		}

		const auto GetNode = [&](const int32 Index)
		{
			return FunctionInfo->LineNumberToSourceNodeMap[CodeOffsets[Index]].Get();
		};

		int32 Index = Algo::LowerBound(CodeOffsets, CodeOffset);
		if (!ensureVoxelSlow(CodeOffsets.IsValidIndex(Index)))
		{
			continue;
		}

		const UEdGraphNode* BlueprintNode = GetNode(Index);

		// Search forward for a node
		while (!BlueprintNode)
		{
			if (!CodeOffsets.IsValidIndex(Index + 1))
			{
				break;
			}

			Index++;
			BlueprintNode = GetNode(Index);
		}

		if (!BlueprintNode)
		{
			continue;
		}

		// With latent nodes the code offset points to a tunnel
		// Walk backwards to the actual function call
		for (int32 InnerIndex = 0; InnerIndex < Index; InnerIndex++)
		{
			BlueprintNode = GetNode(InnerIndex);
			if (BlueprintNode->IsA<UK2Node_Tunnel>())
			{
				continue;
			}

			if (AddedNodes.Contains(BlueprintNode))
			{
				continue;
			}

			AddedNodes.Add(BlueprintNode);
			Callstack.Add(BlueprintNode);
		}
	}

	Message->AddToken(FVoxelMessageTokenFactory::CreateObjectToken(Callstack.Last().Get()));

	const TSharedRef<FVoxelMessageToken_BlueprintCallstack> Token = MakeShared<FVoxelMessageToken_BlueprintCallstack>();
	Token->Callstack = Callstack;
	Token->Message = MakeSharedCopy(*Message);

	Message->AddText(" ");
	Message->AddToken(Token);
}

VOXEL_RUN_ON_STARTUP_GAME()
{
	GVoxelMessageManager->GatherCallstacks.Add([](const TSharedRef<FVoxelMessage>& Message)
	{
		GatherBlueprintCallstack(Message);
	});
}