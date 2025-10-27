// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelUObjectBulkLoader.h"
#include "Serialization/BulkDataWriter.h"

void FVoxelUObjectBulkLoader::Serialize(FArchive& Ar, UObject* Owner)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_WRITE_LOCK(BulkData_CriticalSection);

	using FVersion = DECLARE_VOXEL_VERSION
	(
		FirstVersion
	);

	int32 Version = FVersion::LatestVersion;
	Ar << Version;
	ensure(Version == FVersion::LatestVersion);

	SerializeMetadata(Ar);

	if (FVoxelUtilities::ShouldSerializeBulkData(Ar))
	{
		BulkData_RequiresLock.SerializeWithFlags(Ar, Owner, BULKDATA_Force_NOT_InlinePayload);
	}
}

TVoxelFuture<TSharedPtr<const TVoxelArray64<uint8>>> FVoxelUObjectBulkLoader::ReadRangeAsync(const int64 Offset, const int64 Length)
{
	VOXEL_SCOPE_READ_LOCK(BulkData_CriticalSection);

	return FVoxelUtilities::ReadBulkDataAsync(
		BulkData_RequiresLock,
		Offset,
		Length);
}

bool FVoxelUObjectBulkLoader::ReadRange(const int64 Offset, const TVoxelArrayView<uint8> OutData)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FReadLock> ReadLock = GetReadLock();
	if (!ensure(ReadLock))
	{
		return false;
	}

	if (!ensure(ReadLock->Data.IsValidSlice(Offset, OutData.Num())))
	{
		return false;
	}

	FVoxelUtilities::Memcpy(
		OutData,
		ReadLock->Data.Slice(Offset, OutData.Num()));

	return true;
}

bool FVoxelUObjectBulkLoader::AppendRange(const int64 CurrentSize, const TConstVoxelArrayView<uint8> NewData)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_WRITE_LOCK(BulkData_CriticalSection);

	if (!ensure(BulkData_RequiresLock.GetBulkDataSize() == CurrentSize))
	{
		return false;
	}

	BulkData_RequiresLock.Lock(LOCK_READ_WRITE);
	ON_SCOPE_EXIT
	{
		BulkData_RequiresLock.Unlock();
	};

	void* Data = BulkData_RequiresLock.Realloc(CurrentSize + NewData.Num());
	if (!ensure(Data))
	{
		return false;
	}

	FMemory::Memcpy(
		static_cast<uint8*>(Data) + CurrentSize,
		NewData.GetData(),
		NewData.Num());

	return true;
}

bool FVoxelUObjectBulkLoader::TruncateAndWrite(const TConstVoxelArrayView<uint8> NewData)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_WRITE_LOCK(BulkData_CriticalSection);

	BulkData_RequiresLock.Lock(LOCK_READ_WRITE);
	ON_SCOPE_EXIT
	{
		BulkData_RequiresLock.Unlock();
	};

	void* Data = BulkData_RequiresLock.Realloc(NewData.Num());
	if (!ensure(Data))
	{
		return false;
	}

	FMemory::Memcpy(
		Data,
		NewData.GetData(),
		NewData.Num());

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelUObjectBulkLoader::FReadLock> FVoxelUObjectBulkLoader::GetReadLock()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(GetReadLock_CriticalSection);

	if (const TSharedPtr<FReadLock> ReadLock = WeakReadLock.Pin())
	{
		return ReadLock.ToSharedRef();
	}

	BulkData_CriticalSection.WriteLock();

	const void* Data = BulkData_RequiresLock.LockReadOnly();
	if (!ensure(Data))
	{
		BulkData_CriticalSection.WriteUnlock();
		return {};
	}

	const TSharedRef<FReadLock> ReadLock(new FReadLock
	{
		SharedThis(this),
		TConstVoxelArrayView64<uint8>(static_cast<const uint8*>(Data), BulkData_RequiresLock.GetBulkDataSize())
	});

	WeakReadLock = ReadLock;

	return ReadLock;
}