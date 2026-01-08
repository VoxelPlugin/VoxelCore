// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkPtr.h"
#include "Bulk/VoxelBulkLoader.h"
#include "Bulk/VoxelBulkPtrArchives.h"
#include "VoxelTaskContext.h"

// Timestamp used to ensure we ignore newly loaded data when computing a state
// This is critical to have a consistent distance field, ensuring no holes
FVoxelCounter64 GVoxelBulkDataTimestamp = 1000;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelBulkHasherArchive : public FMemoryArchive
{
public:
	FVoxelBulkHasherArchive()
	{
		SetIsSaving(true);
		SetIsPersistent(true);
		SetWantBinaryPropertySerialization(true);
	}

	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkHasherArchive";
	}
	virtual void Serialize(void* V, const int64 Length) override
	{
		VOXEL_FUNCTION_COUNTER_NUM(Length, 256);
		Hasher.Update(static_cast<const uint8*>(V), uint64(Length));
	}
	//~ End FArchive Interface

	FVoxelBulkHash Finalize()
	{
		Hasher.Final();

		FVoxelBulkHash Result;
		FMemory::Memcpy(&Result, Hasher.m_digest, 16);
		return Result;
	}

private:
	FSHA1 Hasher;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBulkPtr::FVoxelBulkPtr(const TSharedRef<const FVoxelBulkData>& Data)
{
	VOXEL_FUNCTION_COUNTER();
	FVoxelTaskScope Scope(*GVoxelGlobalTaskContext);

	FVoxelBulkHasherArchive Hasher;
	ConstCast(*Data).SerializeAsBytes(Hasher);

	const FVoxelBulkHash Hash = Hasher.Finalize();

	if (VOXEL_DEBUG)
	{
		FVoxelBulkPtrWriter Writer;
		ConstCast(*Data).SerializeAsBytes(Writer);
		check(FVoxelBulkHash::Create(Writer.Bytes) == Hash);
	}

	Inner = new FInner(*Data->GetStruct(), Hash);
	Inner->Future = Data;
	Inner->LoadTimestamp = GVoxelBulkDataTimestamp.Increment_ReturnNew();
}

FVoxelBulkPtr::FVoxelBulkPtr(
	const UScriptStruct& Struct,
	const FVoxelBulkHash& Hash)
{
	checkVoxelSlow(!Hash.IsNull());
	Inner = new FInner(Struct, Hash);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBulkPtr::FullyLoadSync(IVoxelBulkLoader& Loader) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!IsLoaded())
	{
		LoadSync(Loader);
	}

	for (const FVoxelBulkPtr& BulkPtr : GetDependencies())
	{
		BulkPtr.FullyLoadSync(Loader);
	}
}

TVoxelArray<uint8> FVoxelBulkPtr::WriteToBytes() const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsLoaded());

	FVoxelBulkPtrWriter Writer;
	ConstCast(Get()).SerializeAsBytes(Writer);
	checkVoxelSlow(FVoxelBulkHash::Create(Writer.Bytes) == GetHash());

	return TVoxelArray<uint8>(MoveTemp(Writer.Bytes));
}

class FVoxelBulkPtrDependencyCollector : public FArchive
{
public:
	TVoxelArray<FVoxelBulkPtr> BulkPtrs;

	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrDependencyCollector";
	}
	//~ End FArchive Interface
};

TVoxelArray<FVoxelBulkPtr> FVoxelBulkPtr::GetDependencies() const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsLoaded());

	FVoxelBulkPtrDependencyCollector Collector;
	Collector.SetIsSaving(true);
	Collector.BulkPtrs.Reserve(64);

	ConstCast(Get()).Serialize(Collector);

	return MoveTemp(Collector.BulkPtrs);
}

FVoxelBulkPtr FVoxelBulkPtr::LoadFromBytes(
	const UScriptStruct& Struct,
	const TConstVoxelArrayView<uint8> Bytes)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelBulkData> BulkData = MakeSharedStruct<FVoxelBulkData>(&Struct);

	FVoxelBulkPtrReader Reader(Bytes);
	BulkData->SerializeAsBytes(Reader);

	if (!ensure(Reader.IsAtEndWithoutError()))
	{
		return {};
	}

	const FVoxelBulkPtr BulkPtr(BulkData);
	checkVoxelSlow(FVoxelBulkHash::Create(Bytes) == BulkPtr.GetHash());
	return BulkPtr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBulkPtr::Serialize(FArchive& Ar, const UScriptStruct& Struct)
{
	VOXEL_FUNCTION_COUNTER();

	const FString ArchiveName = Ar.GetArchiveName();

	if (ArchiveName == "FVoxelBulkPtrDependencyCollector")
	{
		if (IsSet())
		{
			static_cast<FVoxelBulkPtrDependencyCollector&>(Ar).BulkPtrs.Add(*this);
		}

		return;
	}

	if (ArchiveName == "FVoxelBulkPtrWriter" ||
		ArchiveName == "FVoxelBulkHasherArchive" ||
		ArchiveName == "FVoxelBulkPtrShallowArchive")
	{
		FVoxelBulkHash Hash = GetHash();
		Ar << Hash;
		return;
	}

	if (ArchiveName == "FVoxelBulkPtrReader" ||
		ArchiveName == "FVoxelBulkPtrShallowArchive")
	{
		check(Ar.IsLoading());

		FVoxelBulkHash Hash;
		Ar << Hash;

		if (Hash.IsNull())
		{
			*this = {};
		}
		else
		{
			*this = FVoxelBulkPtr(Struct, Hash);
		}
		return;
	}

	if (Ar.IsLoading())
	{
		const TSharedRef<FVoxelBulkData> Result = MakeSharedStruct<FVoxelBulkData>(&Struct);
		Result->Serialize(Ar);
		*this = FVoxelBulkPtr(Result);
	}
	else
	{
		if (!ensureMsgf(IsLoaded(), TEXT("Cannot serialize an unloaded BulkPtr")))
		{
			Ar.SetError();
			return;
		}

		ConstCast(Get()).Serialize(Ar);
	}
}

void FVoxelBulkPtr::ShallowSerialize(
	FArchive& Ar,
	const UScriptStruct& Struct)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelBulkPtrShallowArchive ShallowArchive(Ar);

	if (Ar.IsLoading())
	{
		const TSharedRef<FVoxelBulkData> Result = MakeSharedStruct<FVoxelBulkData>(&Struct);
		ShallowArchive.SetIsLoading(true);
		Result->Serialize(ShallowArchive);
		*this = FVoxelBulkPtr(Result);
	}
	else
	{
		if (!ensureMsgf(IsLoaded(), TEXT("Cannot serialize an unloaded BulkPtr")))
		{
			Ar.SetError();
			return;
		}

		ShallowArchive.SetIsSaving(true);
		ConstCast(Get()).Serialize(ShallowArchive);
	}
}

void FVoxelBulkPtr::GatherObjects(TVoxelSet<TVoxelObjectPtr<UObject>>& OutObjects) const
{
	if (!IsLoaded())
	{
		return;
	}

	Get().GatherObjects(OutObjects);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelBulkPtr::GetGlobalTimestamp()
{
	return GVoxelBulkDataTimestamp.Get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFuture<const FVoxelBulkData> FVoxelBulkPtr::FInner::Load(IVoxelBulkLoader& Loader)
{
	FVoxelTaskScope Scope(*GVoxelGlobalTaskContext);

	if (Future)
	{
		return Future.GetFuture();
	}

	VOXEL_SCOPE_LOCK_ATOMIC(bIsLocked);

	if (Future)
	{
		return Future.GetFuture();
	}

	Future =
		Loader.LoadBulkData(Hash)
		.Then_AsyncThread(MakeStrongPtrLambda(this, [this](const TSharedPtr<const TVoxelArray64<uint8>>& Data)
		{
			if (!ensure(Data))
			{
				LOG_VOXEL(Error, "Failed to load bulk data for hash %s struct %s", *Hash.ToString(), *Struct.GetName());
				return MakeSharedStruct<FVoxelBulkData>(&Struct);
			}

			const TSharedRef<FVoxelBulkData> Result = MakeSharedStruct<FVoxelBulkData>(&Struct);

			FVoxelBulkPtrReader Reader(*Data);
			Result->SerializeAsBytes(Reader);

			if (!ensure(Reader.IsAtEndWithoutError()))
			{
				LOG_VOXEL(Error, "Failed to deserialize bulk data for hash %s struct %s", *Hash.ToString(), *Struct.GetName());
				return MakeSharedStruct<FVoxelBulkData>(&Struct);
			}

			if (VOXEL_DEBUG)
			{
				const FVoxelBulkPtr NewBulkPtr = FVoxelBulkPtr(Result);
				check(Hash == NewBulkPtr.GetHash());
			}

			return Result;
		}));

	Future->Then_AnyThread(MakeStrongPtrLambda(this, [this](const FVoxelBulkData&)
	{
		checkVoxelSlow(LoadTimestamp == -1);
		LoadTimestamp = GVoxelBulkDataTimestamp.Increment_ReturnNew();
	}));

	return Future.GetFuture();
}

TSharedRef<const FVoxelBulkData> FVoxelBulkPtr::FInner::LoadSync(IVoxelBulkLoader& Loader) const
{
	VOXEL_SCOPE_LOCK_ATOMIC(bIsLocked);

	if (Future &&
		Future->IsComplete())
	{
		return Future->GetSharedValueChecked();
	}

	const TSharedPtr<const TVoxelArray64<uint8>> Data = Loader.LoadBulkDataSync(Hash);

	if (!ensure(Data))
	{
		LOG_VOXEL(Error, "Failed to load bulk data synchronously for hash %s struct %s", *Hash.ToString(), *Struct.GetName());
		return MakeSharedStruct<FVoxelBulkData>(&Struct);
	}

	const TSharedRef<FVoxelBulkData> Result = MakeSharedStruct<FVoxelBulkData>(&Struct);

	FVoxelBulkPtrReader Reader(*Data);
	Result->SerializeAsBytes(Reader);

	if (!ensure(Reader.IsAtEndWithoutError()))
	{
		LOG_VOXEL(Error, "Failed to deserialize bulk data for hash %s struct %s", *Hash.ToString(), *Struct.GetName());
		return MakeSharedStruct<FVoxelBulkData>(&Struct);
	}

	if (VOXEL_DEBUG)
	{
		const FVoxelBulkPtr NewBulkPtr = FVoxelBulkPtr(Result);
		check(Hash == NewBulkPtr.GetHash());
	}

	return Result;
}