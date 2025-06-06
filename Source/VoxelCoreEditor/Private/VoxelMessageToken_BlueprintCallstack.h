// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelMessage.h"
#include "VoxelMessageToken_BlueprintCallstack.generated.h"

USTRUCT()
struct FVoxelMessageToken_BlueprintCallstack : public FVoxelMessageToken
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	TArray<TWeakObjectPtr<const UEdGraphNode>> Callstack;
	TSharedPtr<const FVoxelMessage> Message;

	//~ Begin FVoxelMessageToken Interface
	virtual uint32 GetHash() const override;
	virtual FString ToString() const override;
	virtual TSharedRef<IMessageToken> GetMessageToken() const override;
	//~ End FVoxelMessageToken Interface
};