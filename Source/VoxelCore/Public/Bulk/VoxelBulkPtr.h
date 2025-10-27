// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Bulk/VoxelBulkData.h"
#include "Bulk/VoxelBulkHash.h"

class IVoxelBulkLoader;
class FVoxelBulkDataCollector;

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

	TVoxelArray<FVoxelBulkPtr> GetDependencies() const;
	void Serialize(FArchive& Ar, const UScriptStruct& Struct);

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

template<typename Type>
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

	void Serialize(FArchive& Ar)
	{
		// Otherwise StaticStructFast might be incorrect
		checkStatic(std::is_final_v<Type>);

		FVoxelBulkPtr::Serialize(Ar, *StaticStructFast<Type>());
	}

public:
	FORCEINLINE const Type& Get() const
	{
		return static_cast<const Type&>(FVoxelBulkPtr::Get());
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

public:
	friend void operator<<(FArchive& Ar, TVoxelBulkPtr& BulkPtr)
	{
		BulkPtr.Serialize(Ar);
	}
};