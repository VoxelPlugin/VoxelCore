// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Bulk/VoxelBulkArchive.h"

class VOXELCORE_API FVoxelUObjectBulkLoader : public FVoxelBulkArchive
{
public:
	FVoxelUObjectBulkLoader() = default;

	void Serialize(FArchive& Ar, UObject* Owner);

protected:
	//~ Begin FVoxelBulkArchive Interface
	virtual TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> ReadRangeAsync(int64 Offset, int64 Length) override;
	virtual bool ReadRange(int64 Offset, TVoxelArrayView<uint8> OutData) override;
	virtual bool AppendRange(int64 CurrentSize, TConstVoxelArrayView<uint8> NewData) override;
	virtual bool TruncateAndWrite(TConstVoxelArrayView<uint8> NewData) override;
	//~ End FVoxelBulkArchive Interface

private:
	FVoxelSharedCriticalSection BulkData_CriticalSection;
	FByteBulkData BulkData_RequiresLock;

private:
	struct FReadLock
	{
		const TSharedRef<FVoxelUObjectBulkLoader> Loader;
		const TConstVoxelArrayView64<uint8> Data;

		~FReadLock()
		{
			Loader->BulkData_RequiresLock.Unlock();
			Loader->BulkData_CriticalSection.WriteUnlock();
		}
	};
	FVoxelCriticalSection GetReadLock_CriticalSection;
	TWeakPtr<FReadLock> WeakReadLock;

	TSharedPtr<FReadLock> GetReadLock();
};