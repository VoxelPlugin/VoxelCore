// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Bulk/VoxelBulkLoader.h"

struct FVoxelBulkPtr;

class VOXELCORE_API FVoxelBulkArchive : public IVoxelBulkLoader
{
public:
	FVoxelBulkArchive() = default;

	bool Save(
		const TVoxelArray<FVoxelBulkPtr>& NewRoots,
		int64 MaxWasteInBytes);

protected:
	//~ Begin IVoxelBulkLoader Interface
	virtual TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> LoadBulkDataImpl(const FVoxelBulkHash& Hash) final override;
	virtual TSharedPtr<const TVoxelArray64<uint8>> LoadBulkDataSyncImpl(const FVoxelBulkHash& Hash) final override;
	//~ End IVoxelBulkLoader Interface

	void SerializeMetadata(FArchive& Ar);

protected:
	virtual TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> ReadRangeAsync(int64 Offset, int64 Length) = 0;
	virtual bool ReadRange(int64 Offset, TVoxelArrayView<uint8> OutData) = 0;
	virtual bool AppendRange(int64 CurrentSize, TConstVoxelArrayView<uint8> NewData) = 0;
	virtual bool TruncateAndWrite(TConstVoxelArrayView<uint8> NewData) = 0;

private:
	struct FMetadata
	{
		int64 Offset = 0;
		int64 Length = 0;
		TVoxelArray<FVoxelBulkHash> Dependencies;

		friend void operator<<(FArchive& Ar, FMetadata& Metadata)
		{
			Ar << Metadata.Offset;
			Ar << Metadata.Length;
			Ar << Metadata.Dependencies;
		}
	};
	FVoxelSharedCriticalSection HashToMetadata_CriticalSection;
	int64 TotalSize = 0;
	TVoxelMap<FVoxelBulkHash, FMetadata> HashToMetadata_RequiresLock;

	bool GatherHashes_RequiresLock(
		TConstVoxelArrayView<FVoxelBulkPtr> Roots,
		TVoxelSet<FVoxelBulkHash>& Hashes,
		TVoxelMap<FVoxelBulkHash, FVoxelBulkPtr>& HashToBulkPtr) const;

	bool WriteBulkPtrs_RequiresLock(TConstVoxelArrayView<FVoxelBulkPtr> BulkPtrs);

	bool Reallocate_RequiresLock(const TVoxelSet<FVoxelBulkHash>& HashesToKeep);
};