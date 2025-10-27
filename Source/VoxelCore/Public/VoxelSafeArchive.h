// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelSafeArchive : public FArchiveProxy
{
public:
	using FArchiveProxy::FArchiveProxy;

	FORCEINLINE bool HasLoadingError() const
	{
		return IsLoading() && IsError();
	}

public:
	//~ Begin FArchive Interface
	virtual void SerializeIntPacked64(uint64& Value) override;
	virtual FArchive& operator<<(FName& Value) override;
	virtual FArchive& operator<<(FText& Value) override;
	virtual FArchive& operator<<(UObject*& Value) override;
	virtual FArchive& operator<<(FObjectPtr& Value) override;
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override;
	virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FArchive& operator<<(FField*& Value) override;
	virtual void Serialize(void* V, const int64 Length) override;
	virtual void SerializeBits(void* V, const int64 LengthBits) override;
	virtual void SerializeInt(uint32& Value, const uint32 Max) override;
	virtual void SerializeIntPacked(uint32& Value) override;
	//~ End FArchive Interface
};