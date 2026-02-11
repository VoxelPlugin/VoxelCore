// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBulkHint.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelBulkHintData : public FVoxelVirtualStruct
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	virtual void Serialize(FArchive& Ar) VOXEL_PURE_VIRTUAL();
	virtual FString ToString() const VOXEL_PURE_VIRTUAL({});
};

struct VOXELCORE_API FVoxelBulkHint
{
	TSharedPtr<const FVoxelBulkHintData> Data;

	void Serialize(FArchive& Ar);
};