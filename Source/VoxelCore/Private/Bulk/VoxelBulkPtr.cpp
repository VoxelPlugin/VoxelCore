// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkPtr.h"
#include "Bulk/VoxelBulkLoader.h"
#include "Bulk/VoxelBulkPtrArchives.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

// Timestamp used to ensure we ignore newly loaded data when computing a state
// This is critical to have a consistent distance field, ensuring no holes
FVoxelCounter64 GVoxelBulkDataTimestamp = 1000;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelBulkHasherArchive : public FMemoryArchive
{
public:
	FVoxelBulkHasherArchive() = default;

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

	FVoxelBulkHasherArchive Hasher;
	{
		VOXEL_SCOPE_COUNTER("Serialize");

		FObjectAndNameAsStringProxyArchive Proxy(Hasher, true);
		ConstCast(*Data).Serialize(Proxy);
	}
	const FVoxelBulkHash Hash = Hasher.Finalize();

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
	checkVoxelSlow(IsLoaded());

	FVoxelBulkPtrDependencyCollector Collector;
	Collector.BulkPtrs.Reserve(64);

	ConstCast(Get()).Serialize(Collector);

	return MoveTemp(Collector.BulkPtrs);
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
		ArchiveName == "FVoxelBulkHasherArchive")
	{
		FVoxelBulkHash Hash = GetHash();
		Ar << Hash;
		return;
	}

	if (ArchiveName == "FVoxelBulkPtrReader")
	{
		check(Ar.IsLoading());

		FVoxelBulkHash Hash;
		Ar << Hash;
		*this = FVoxelBulkPtr(Struct, Hash);
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
			{
				FObjectAndNameAsStringProxyArchive Proxy(Reader, true);
				Result->Serialize(Proxy);
			}

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
	{
		FObjectAndNameAsStringProxyArchive Proxy(Reader, true);
		Result->Serialize(Proxy);
	}

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