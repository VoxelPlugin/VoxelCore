// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Bulk/VoxelBulkData.h"
#include "Bulk/VoxelBulkHash.h"

class IVoxelBulkLoader;
class FVoxelBulkDataCollector;

template<typename Type>
struct TVoxelBulkRef;

struct VOXELCORE_API FVoxelBulkPtr
{
public:
	enum class EState : uint8
	{
		PendingLoad,
		Loading,
		Loaded
	};

	FVoxelBulkPtr() = default;
	explicit FVoxelBulkPtr(const TSharedRef<const FVoxelBulkData>& Data);

	FVoxelBulkPtr(
		const UScriptStruct& Struct,
		const FVoxelBulkHash& Hash);

public:
	void FullyLoadSync(IVoxelBulkLoader& Loader) const;
	TVoxelArray<uint8> WriteToBytes() const;
	TVoxelArray<FVoxelBulkPtr> GetDependencies() const;

	static FVoxelBulkPtr LoadFromBytes(
		const UScriptStruct& Struct,
		TConstVoxelArrayView<uint8> Bytes);

public:
	void Serialize(
		FArchive& Ar,
		const UScriptStruct& Struct);

	void ShallowSerialize(
		FArchive& Ar,
		const UScriptStruct& Struct);

public:
	void GatherObjects(TVoxelSet<TVoxelObjectPtr<UObject>>& OutObjects) const;

public:
	FORCEINLINE bool IsSet() const
	{
		return Inner.IsValid();
	}
	FORCEINLINE bool IsLoaded(const int64 Timestamp = MAX_int64) const
	{
		return
			Inner &&
			Inner->Future.IsSet() &&
			Inner->Future->IsComplete() &&
			Inner->LoadTimestamp <= Timestamp;
	}
	FORCEINLINE const FVoxelBulkData& Get() const
	{
		checkVoxelSlow(IsLoaded());
		return Inner->Future->GetValueChecked();
	}
	FORCEINLINE TSharedRef<const FVoxelBulkData> GetShared() const
	{
		checkVoxelSlow(IsLoaded());
		return Inner->Future->GetSharedValueChecked();
	}
	FORCEINLINE FVoxelBulkHash GetHash() const
	{
		return Inner->Hash;
	}
	FORCEINLINE FVoxelBulkHash GetHashOrNull() const
	{
		return Inner ? Inner->Hash : FVoxelBulkHash();
	}
	FORCEINLINE TVoxelFuture<const FVoxelBulkData> Load(IVoxelBulkLoader& Loader) const
	{
		return Inner->Load(Loader);
	}
	FORCEINLINE TSharedRef<const FVoxelBulkData> LoadSync(IVoxelBulkLoader& Loader) const
	{
		return Inner->LoadSync(Loader);
	}

	FORCEINLINE operator bool() const
	{
		return IsSet();
	}
	FORCEINLINE const FVoxelBulkData* operator->() const
	{
		return &Get();
	}
	FORCEINLINE const FVoxelBulkData& operator*() const
	{
		return Get();
	}

	bool operator==(const FVoxelBulkPtr&) const = delete;

public:
	static int64 GetGlobalTimestamp();

private:
	struct VOXELCORE_API FInner : public TVoxelRefCountThis<FInner>
	{
		mutable TVoxelAtomic<bool> bIsLocked;
		const UScriptStruct& Struct;
		const FVoxelBulkHash Hash;
		int64 LoadTimestamp = -1;
		TVoxelNullableFuture<const FVoxelBulkData> Future;

		explicit FInner(
			const UScriptStruct& Struct,
			const FVoxelBulkHash& Hash)
			: Struct(Struct)
			, Hash(Hash)
		{
		}

		TVoxelFuture<const FVoxelBulkData> Load(IVoxelBulkLoader& Loader);
		TSharedRef<const FVoxelBulkData> LoadSync(IVoxelBulkLoader& Loader) const;
	};
	checkStatic(sizeof(FInner) == 48);

	TVoxelRefCountPtr<FInner> Inner;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Type>
requires (!std::is_const_v<Type>)
struct TVoxelBulkPtr : FVoxelBulkPtr
{
public:
	TVoxelBulkPtr() = default;
	TVoxelBulkPtr(decltype(nullptr))
	{
	}

	explicit TVoxelBulkPtr(const TSharedRef<const Type>& Data)
		: FVoxelBulkPtr(Data)
	{
	}
	explicit TVoxelBulkPtr(const FVoxelBulkHash& Hash)
		: FVoxelBulkPtr(*StaticStructFast<Type>(), Hash)
	{
		// Otherwise StaticStructFast might be incorrect
		checkStatic(std::is_final_v<Type>);
	}

public:
	static TVoxelBulkPtr<Type> LoadFromBytes(const TConstVoxelArrayView<uint8> Bytes)
	{
		return ReinterpretCastRef<TVoxelBulkPtr<Type>>(FVoxelBulkPtr::LoadFromBytes(
			*StaticStructFast<Type>(),
			Bytes));
	}

public:
	void Serialize(FArchive& Ar)
	{
		// Otherwise StaticStructFast might be incorrect
		checkStatic(std::is_final_v<Type>);

		FVoxelBulkPtr::Serialize(Ar, *StaticStructFast<Type>());
	}
	void ShallowSerialize(FArchive& Ar)
	{
		// Otherwise StaticStructFast might be incorrect
		checkStatic(std::is_final_v<Type>);

		FVoxelBulkPtr::ShallowSerialize(Ar, *StaticStructFast<Type>());
	}

public:
	void GatherObjects(TVoxelSet<TVoxelObjectPtr<UObject>>& OutObjects) const
	{
		FVoxelBulkPtr::GatherObjects(OutObjects);
	}

public:
	FORCEINLINE const Type& Get() const
	{
		return CastStructChecked<Type>(FVoxelBulkPtr::Get());
	}
	FORCEINLINE TSharedRef<const Type> GetShared() const
	{
		return CastStructChecked<Type>(FVoxelBulkPtr::GetShared());
	}
	FORCEINLINE TVoxelFuture<const Type> Load(IVoxelBulkLoader& Loader) const
	{
		return ReinterpretCastRef<TVoxelFuture<const Type>>(FVoxelBulkPtr::Load(Loader));
	}
	FORCEINLINE TSharedRef<const Type> LoadSync(IVoxelBulkLoader& Loader) const
	{
		return CastStructChecked<Type>(FVoxelBulkPtr::LoadSync(Loader));
	}
	FORCEINLINE const Type* operator->() const
	{
		return &Get();
	}

	FORCEINLINE const TVoxelBulkRef<Type>& ToBulkRef() const
	{
		checkVoxelSlow(IsLoaded());
		return ReinterpretCastRef<TVoxelBulkRef<Type>>(*this);
	}

public:
	friend void operator<<(FArchive& Ar, TVoxelBulkPtr& BulkPtr)
	{
		BulkPtr.Serialize(Ar);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Type>
requires (!std::is_const_v<Type>)
struct TVoxelBulkRef<Type> : TVoxelBulkPtr<Type>
{
public:
	explicit TVoxelBulkRef(const TSharedRef<const Type>& Data)
		: TVoxelBulkPtr<Type>(Data)
	{
	}

private:
	using TVoxelBulkPtr<Type>::Load;
	using TVoxelBulkPtr<Type>::LoadSync;
	using TVoxelBulkPtr<Type>::IsSet;
	using TVoxelBulkPtr<Type>::IsLoaded;
};

template<typename Type>
requires (!std::is_const_v<Type>)
FORCEINLINE TVoxelBulkRef<Type> MakeVoxelBulkRef(const TSharedRef<Type>& Data)
{
	return TVoxelBulkRef<Type>(Data);
}
template<typename Type>
requires (!std::is_const_v<Type>)
FORCEINLINE TVoxelBulkRef<Type> MakeVoxelBulkRef(const TSharedRef<const Type>& Data)
{
	return TVoxelBulkRef<Type>(Data);
}