// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBulkData.generated.h"

struct FVoxelBulkPtr;
struct FVoxelBulkHash;
class FVoxelBulkHasher;

USTRUCT()
struct VOXELCORE_API FVoxelBulkData
	: public FVoxelVirtualStruct
#if CPP
	, public TSharedFromThis<FVoxelBulkData>
#endif
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	FVoxelBulkData() = default;

	void SerializeAsBytes(FArchive& Ar);
	virtual void Serialize(FArchive& Ar) VOXEL_PURE_VIRTUAL();
	virtual void GatherObjects(TVoxelSet<TVoxelObjectPtr<UObject>>& OutObjects) const VOXEL_PURE_VIRTUAL();
};