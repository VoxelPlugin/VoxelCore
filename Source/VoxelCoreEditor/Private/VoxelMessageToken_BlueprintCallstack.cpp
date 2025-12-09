// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMessageToken_BlueprintCallstack.h"
#include "SVoxelCallstack.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Logging/TokenizedMessage.h"
#include "Framework/Docking/TabManager.h"

uint32 FVoxelMessageToken_BlueprintCallstack::GetHash() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelInlineArray<uint32, 64> Hashes;
	for (const TWeakObjectPtr<const UEdGraphNode>& WeakNode : Callstack)
	{
		Hashes.Add(GetTypeHash(WeakNode));
	}
	return FVoxelUtilities::MurmurHashView(Hashes);
}

FString FVoxelMessageToken_BlueprintCallstack::ToString() const
{
	VOXEL_FUNCTION_COUNTER();

	FString Result;
	for (int32 Index = 0; Index < Callstack.Num(); Index++)
	{
		const UEdGraphNode* Node = Callstack[Index].Get();
		if (!Node)
		{
			continue;
		}

		if (Index > 0)
		{
			Result += "->";
		}
		Result += FVoxelUtilities::GetReadableName(Node);
	}

	return "\nCallstack: " + Result;
}

TSharedRef<IMessageToken> FVoxelMessageToken_BlueprintCallstack::GetMessageToken() const
{
	return FActionToken::Create(
		INVTEXT("View Callstack"),
		INVTEXT("View callstack"),
		MakeLambdaDelegate([Callstack = Callstack, Title = Message->ToString()]
		{
			SVoxelCallstack::CreatePopup(
				Title,
				[Callstack]
				{
					TArray<TSharedPtr<FVoxelCallstackEntry>> Result;

					TMap<const UObject*, TSharedPtr<FVoxelCallstackEntry>> ObjectToEntry;

					const TWeakObjectPtr<const UEdGraphNode> ErrorNode = Callstack.Last();

					int32 Index = 1;
					for (const TWeakObjectPtr<const UEdGraphNode>& WeakNode : Callstack)
					{
						const UEdGraphNode* Node = WeakNode.Get();
						if (!Node)
						{
							continue;
						}

						const UBlueprint* Blueprint = Node->GetTypedOuter<UBlueprint>();
						TSharedPtr<FVoxelCallstackEntry> BlueprintEntry = ObjectToEntry.FindRef(Blueprint);

						if (!BlueprintEntry)
						{
							BlueprintEntry = MakeShared<FVoxelCallstackObjectEntry>(
								Blueprint,
								FVoxelUtilities::GetReadableName(Blueprint),
								"Blueprint: ",
								FVoxelCallstackEntry::EType::Subdued);
							Result.Add(BlueprintEntry);
							ObjectToEntry.Add(Blueprint, BlueprintEntry);
						}

						UEdGraph* Graph = Node->GetGraph();
						TSharedPtr<FVoxelCallstackEntry> GraphEntry = ObjectToEntry.FindRef(Graph);
						if (!GraphEntry)
						{
							GraphEntry = MakeShared<FVoxelCallstackObjectEntry>(
								Graph,
								Graph->GetName(),
								"Function: ",
								FVoxelCallstackEntry::EType::Subdued);
							BlueprintEntry->Children.Add(GraphEntry);
							ObjectToEntry.Add(Graph, GraphEntry);
						}

						TSharedRef<FVoxelCallstackEntry> NodeEntry = MakeShared<FVoxelCallstackObjectEntry>(
								Node,
								FVoxelUtilities::GetReadableName(Node),
								LexToString(Index++) + ". ",
								Node == ErrorNode
								? FVoxelCallstackEntry::EType::Marked
								: FVoxelCallstackEntry::EType::Default);
						GraphEntry->Children.Add(NodeEntry);
					}

					return Result;
				});
		}));
}