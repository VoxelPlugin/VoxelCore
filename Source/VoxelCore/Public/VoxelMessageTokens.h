// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMessage.h"
#include "EdGraph/EdGraphPin.h"
#include "VoxelMessageTokens.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelMessageToken_Text : public FVoxelMessageToken
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	FString Text;

	//~ Begin FVoxelMessageToken Interface
	virtual uint32 GetHash() const override;
	virtual FString ToString() const override;
	virtual TSharedRef<IMessageToken> GetMessageToken() const override;
	virtual bool TryMerge(const FVoxelMessageToken& Other) override;
	//~ End FVoxelMessageToken Interface
};

USTRUCT()
struct VOXELCORE_API FVoxelMessageToken_Object : public FVoxelMessageToken
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	FVoxelObjectPtr WeakObject;

	//~ Begin FVoxelMessageToken Interface
	virtual uint32 GetHash() const override;
	virtual FString ToString() const override;
	virtual TSharedRef<IMessageToken> GetMessageToken() const override;
	virtual void GetObjects(TSet<const UObject*>& OutObjects) const override;
	//~ End FVoxelMessageToken Interface
};

USTRUCT()
struct VOXELCORE_API FVoxelMessageToken_Pin : public FVoxelMessageToken
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	FEdGraphPinReference PinReference;

	//~ Begin FVoxelMessageToken Interface
	virtual uint32 GetHash() const override;
	virtual FString ToString() const override;
	virtual TSharedRef<IMessageToken> GetMessageToken() const override;
	virtual void GetObjects(TSet<const UObject*>& OutObjects) const override;
	//~ End FVoxelMessageToken Interface
};