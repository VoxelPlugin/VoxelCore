// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Bulk/VoxelBulkLoader.h"

class FVoxelDummyBulkLoader : public IVoxelBulkLoader
{
public:
	FVoxelDummyBulkLoader() = default;

	//~ Begin IVoxelBulkLoader Interface
	virtual TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> LoadBulkDataImpl(const FVoxelBulkHash& Hash) override
	{
		return {};
	}
	virtual TSharedPtr<const TVoxelArray64<uint8>> LoadBulkDataSyncImpl(const FVoxelBulkHash& Hash) override
	{
		return {};
	}
	//~ End IVoxelBulkLoader Interface
};