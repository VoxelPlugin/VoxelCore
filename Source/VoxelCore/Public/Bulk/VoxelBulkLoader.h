// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBulkHash.h"

class VOXELCORE_API IVoxelBulkLoader : public TSharedFromThis<IVoxelBulkLoader>
{
public:
	IVoxelBulkLoader() = default;
	virtual ~IVoxelBulkLoader() = default;

public:
	TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> LoadBulkData(const FVoxelBulkHash& Hash);
	TSharedPtr<const TVoxelArray64<uint8>> LoadBulkDataSync(const FVoxelBulkHash& Hash);

protected:
	virtual TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> LoadBulkDataImpl(const FVoxelBulkHash& Hash) = 0;
	virtual TSharedPtr<const TVoxelArray64<uint8>> LoadBulkDataSyncImpl(const FVoxelBulkHash& Hash) = 0;

private:
	FVoxelCriticalSection HashToFuture_CriticalSection;
	TVoxelMap<FVoxelBulkHash, TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>>> HashToFuture_RequiresLock;
};