// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkArchive.h"
#include "Bulk/VoxelBulkPtr.h"
#include "Bulk/VoxelBulkPtrArchives.h"

bool FVoxelBulkArchive::Save(
	const TVoxelArray<FVoxelBulkPtr>& NewRoots,
	const int64 MaxWasteInBytes)
{
	VOXEL_FUNCTION_COUNTER()
	VOXEL_SCOPE_WRITE_LOCK(HashToMetadata_CriticalSection);

	TVoxelSet<FVoxelBulkHash> Hashes;
	TVoxelMap<FVoxelBulkHash, FVoxelBulkPtr> HashToBulkPtr;

	if (!ensure(GatherHashes_RequiresLock(NewRoots, Hashes, HashToBulkPtr)))
	{
		return false;
	}

	if (!ensure(WriteBulkPtrs_RequiresLock(HashToBulkPtr.ValueArray())))
	{
		return false;
	}

	int64 WastedBytes = 0;
	for (const auto& It : HashToMetadata_RequiresLock)
	{
		if (Hashes.Contains(It.Key))
		{
			continue;
		}

		WastedBytes += It.Value.Length;
	}

	if (WastedBytes < MaxWasteInBytes)
	{
		return true;
	}

	return ensure(Reallocate_RequiresLock(Hashes));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> FVoxelBulkArchive::LoadBulkDataImpl(const FVoxelBulkHash& Hash)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_READ_LOCK(HashToMetadata_CriticalSection);

	const FMetadata* Metadata = HashToMetadata_RequiresLock.Find(Hash);
	if (!ensure(Metadata))
	{
		return {};
	}

	return ReadRangeAsync(Metadata->Offset, Metadata->Length);
}

TSharedPtr<const TVoxelArray64<uint8>> FVoxelBulkArchive::LoadBulkDataSyncImpl(const FVoxelBulkHash& Hash)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_READ_LOCK(HashToMetadata_CriticalSection);

	const FMetadata* Metadata = HashToMetadata_RequiresLock.Find(Hash);
	if (!ensure(Metadata))
	{
		return {};
	}

	TVoxelArray64<uint8> Result;
	FVoxelUtilities::SetNumFast(Result, Metadata->Length);

	if (!ensure(ReadRange(Metadata->Offset, Result)))
	{
		return {};
	}

	return MakeSharedCopy(MoveTemp(Result));
}

void FVoxelBulkArchive::SerializeMetadata(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_WRITE_LOCK(HashToMetadata_CriticalSection);

	using FVersion = DECLARE_VOXEL_VERSION
	(
		FirstVersion
	);

	int32 Version = FVersion::LatestVersion;
	Ar << Version;
	ensure(Version == FVersion::LatestVersion);

	Ar << TotalSize;
	Ar << HashToMetadata_RequiresLock;

	if (VOXEL_DEBUG)
	{
		TVoxelMap<FVoxelBulkHash, FMetadata> HashToMetadata = HashToMetadata_RequiresLock;
		HashToMetadata.ValueSort([](const FMetadata& A, const FMetadata& B)
		{
			return A.Offset < B.Offset;
		});

		int64 Index = 0;
		for (const auto& It : HashToMetadata)
		{
			check(Index == It.Value.Offset);
			Index += It.Value.Length;
		}
		check(Index == TotalSize);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelBulkArchive::GatherHashes_RequiresLock(
	const TConstVoxelArrayView<FVoxelBulkPtr> Roots,
	TVoxelSet<FVoxelBulkHash>& Hashes,
	TVoxelMap<FVoxelBulkHash, FVoxelBulkPtr>& HashToBulkPtr) const
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(HashToMetadata_CriticalSection.IsLocked_Read());

	Hashes.Reserve(16384);
	HashToBulkPtr.Reserve(1024);

	TVoxelArray<FVoxelBulkPtr> BulkPtrQueue;
	BulkPtrQueue.Reserve(256);

	for (const FVoxelBulkPtr& Root : Roots)
	{
		if (ensure(Root.IsSet()) &&
			Hashes.TryAdd(Root.GetHash()))
		{
			BulkPtrQueue.Add(Root);
		}
	}

	TVoxelArray<FVoxelBulkHash> HashQueue;
	HashQueue.Reserve(256);

	while (BulkPtrQueue.Num() > 0)
	{
		const FVoxelBulkPtr BulkPtr = BulkPtrQueue.Pop();
		checkVoxelSlow(BulkPtr.IsSet());
		checkVoxelSlow(Hashes.Contains(BulkPtr.GetHash()));

		if (const FMetadata* Metadata = HashToMetadata_RequiresLock.Find(BulkPtr.GetHash()))
		{
			// If we have metadata, then all our dependencies are known
			for (const FVoxelBulkHash& Dependency : Metadata->Dependencies)
			{
				if (!Hashes.TryAdd(Dependency))
				{
					// Already visited
					continue;
				}

				checkVoxelSlow(HashQueue.Num() == 0);
				HashQueue.Add(Dependency);

				while (HashQueue.Num() > 0)
				{
					const FVoxelBulkHash Hash = HashQueue.Pop();
					checkVoxelSlow(Hashes.Contains(Hash));

					const FMetadata* OtherMetadata = HashToMetadata_RequiresLock.Find(Hash);
					if (!ensure(OtherMetadata))
					{
						// Should never happen
						return false;
					}

					for (const FVoxelBulkHash& OtherDependency : OtherMetadata->Dependencies)
					{
						if (Hashes.TryAdd(OtherDependency))
						{
							HashQueue.Add(OtherDependency);
						}
					}
				}
			}
			continue;
		}

		if (!ensure(BulkPtr.IsLoaded()))
		{
			LOG_VOXEL(Error, "Cannot serialize unknown hash %s: bulk ptr is not loaded", *BulkPtr.GetHash().ToString());
			return false;
		}

		HashToBulkPtr.Add_EnsureNew(BulkPtr.GetHash(), BulkPtr);

		for (const FVoxelBulkPtr& Dependency : BulkPtr.GetDependencies())
		{
			if (ensure(Dependency.IsSet()) &&
				Hashes.TryAdd(Dependency.GetHash()))
			{
				BulkPtrQueue.Add(Dependency);
			}
		}
	}

	if (VOXEL_DEBUG)
	{
		for (const FVoxelBulkHash& Hash : Hashes)
		{
			const bool bHasMetadata = HashToMetadata_RequiresLock.Contains(Hash);
			const bool bHasBulkPtr = HashToBulkPtr.Contains(Hash);

			// Either we were already stored OR we have a loaded bulk ptr
			check(bHasMetadata != bHasBulkPtr);
		}
	}

	return true;
}

bool FVoxelBulkArchive::WriteBulkPtrs_RequiresLock(const TConstVoxelArrayView<FVoxelBulkPtr> BulkPtrs)
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(HashToMetadata_CriticalSection.IsLocked_Write());

	if (BulkPtrs.Num() == 0)
	{
		return true;
	}

	struct FInfo
	{
		TVoxelArray<uint8> Data;
		TVoxelArray<FVoxelBulkHash> Dependencies;
		int64 Offset = 0;
	};
	TVoxelMap<FVoxelBulkHash, FInfo> HashToInfo;

	{
		VOXEL_SCOPE_COUNTER("HashToInfo");

		HashToInfo.Reserve(BulkPtrs.Num());

		FVoxelCriticalSection CriticalSection;

		Voxel::ParallelFor(BulkPtrs, [&](const FVoxelBulkPtr& BulkPtr)
		{
			FInfo Info;
			Info.Data = BulkPtr.WriteToBytes();

			{
				const TVoxelArray<FVoxelBulkPtr> Dependencies = BulkPtr.GetDependencies();
				Info.Dependencies.Reserve(Dependencies.Num());

				for (const FVoxelBulkPtr& Dependency : Dependencies)
				{
					Info.Dependencies.Add(Dependency.GetHash());
				}
			}

			VOXEL_SCOPE_LOCK(CriticalSection);
			HashToInfo.Add_EnsureNew(BulkPtr.GetHash(), MoveTemp(Info));
		});
	}

	int64 NewSize = 0;
	for (auto& It : HashToInfo)
	{
		const FVoxelBulkHash Hash = It.Key;
		FInfo& Info = It.Value;

		Info.Offset = NewSize;
		NewSize += Info.Data.Num();

		HashToMetadata_RequiresLock.Add_EnsureNew(Hash, FMetadata
		{
			TotalSize + Info.Offset,
			Info.Data.Num(),
			MoveTemp(Info.Dependencies)
		});
	}

	TVoxelArray64<uint8> NewData;
	{
		VOXEL_SCOPE_COUNTER("NewData");

		FVoxelUtilities::SetNumFast(NewData, NewSize);

		Voxel::ParallelFor_Values(HashToInfo, [&](const FInfo& Info)
		{
			FVoxelUtilities::Memcpy(
				NewData.View().Slice(Info.Offset, Info.Data.Num()),
				Info.Data);
		});
	}

	if (!ensure(AppendRange(TotalSize, NewData)))
	{
		return false;
	}

	TotalSize += NewSize;
	return true;
}

bool FVoxelBulkArchive::Reallocate_RequiresLock(const TVoxelSet<FVoxelBulkHash>& HashesToKeep)
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(HashToMetadata_CriticalSection.IsLocked_Write());

	int64 NewSize = 0;
	TVoxelMap<FVoxelBulkHash, FMetadata> NewHashToMetadata;
	{
		VOXEL_SCOPE_COUNTER("NewHashToMetadata");

		NewHashToMetadata.Reserve(HashesToKeep.Num());

		for (const FVoxelBulkHash& Hash : HashesToKeep)
		{
			const FMetadata* Metadata = HashToMetadata_RequiresLock.Find(Hash);
			if (!ensure(Metadata))
			{
				return false;
			}

			FMetadata& NewMetadata = NewHashToMetadata.Add_EnsureNew(Hash);
			NewMetadata.Offset = NewSize;
			NewMetadata.Length = Metadata->Length;
			NewMetadata.Dependencies = Metadata->Dependencies;

			NewSize += Metadata->Length;
		}
	}

	TVoxelArray64<uint8> NewData;
	{
		VOXEL_SCOPE_COUNTER("NewData");

		FVoxelUtilities::SetNumFast(NewData, NewSize);

		bool bFailed = false;

		Voxel::ParallelFor(NewHashToMetadata, [&](const TVoxelMapElement<FVoxelBulkHash, FMetadata>& It)
		{
			const FMetadata& OldMetadata = HashToMetadata_RequiresLock[It.Key];
			const FMetadata& NewMetadata = It.Value;

			const bool bSuccess = ReadRange(
				OldMetadata.Offset,
				NewData.View().Slice(NewMetadata.Offset, NewMetadata.Length));

			if (!ensure(bSuccess))
			{
				bFailed = true;
			}
		});

		if (!ensure(!bFailed))
		{
			return false;
		}
	}

	if (!ensure(TruncateAndWrite(NewData)))
	{
		return false;
	}

	TotalSize = NewSize;
	HashToMetadata_RequiresLock = MoveTemp(NewHashToMetadata);

	return true;
}