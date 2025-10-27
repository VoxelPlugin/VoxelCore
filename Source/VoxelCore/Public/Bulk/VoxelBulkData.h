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

	virtual void Serialize(FArchive& Ar) VOXEL_PURE_VIRTUAL();
};