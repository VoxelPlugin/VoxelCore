// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkLoader.h"

TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> IVoxelBulkLoader::LoadBulkData(const FVoxelBulkHash& Hash)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelOptional<TVoxelPromise<TSharedPtr<const TVoxelArray64<uint8>>>> Promise;
	{
		VOXEL_SCOPE_LOCK(HashToFuture_CriticalSection);

		if (const TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>>* Future = HashToFuture_RequiresLock.Find(Hash))
		{
			return *Future;
		}

		HashToFuture_RequiresLock.Add_EnsureNew(Hash, Promise.Emplace());
	}

	Promise->Set(LoadBulkDataImpl(Hash));

	Promise->Then_AsyncThread(MakeWeakPtrLambda(this, [this, Hash](const TSharedPtr<const TVoxelArray64<uint8>>& Data)
	{
		if (Data && VOXEL_DEBUG)
		{
			const FVoxelBulkHash ActualHash = FVoxelBulkHash::Create(*Data);

			if (Hash != ActualHash)
			{
				LOG_VOXEL(Error, "Hash mismatch: expected %s, got %s", *Hash.ToString(), *ActualHash.ToString());
				ensure(false);
			}
		}

		VOXEL_SCOPE_LOCK(HashToFuture_CriticalSection);
		HashToFuture_RequiresLock.Remove(Hash);
	}));

	return Promise.GetValue();
}

TSharedPtr<const TVoxelArray64<uint8>> IVoxelBulkLoader::LoadBulkDataSync(const FVoxelBulkHash& Hash)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<const TVoxelArray64<uint8>> Data = LoadBulkDataSyncImpl(Hash);

	if (Data && VOXEL_DEBUG)
	{
		const FVoxelBulkHash ActualHash = FVoxelBulkHash::Create(*Data);

		if (Hash != ActualHash)
		{
			LOG_VOXEL(Error, "Hash mismatch: expected %s, got %s", *Hash.ToString(), *ActualHash.ToString());
			ensure(false);
		}
	}

	return Data;
}