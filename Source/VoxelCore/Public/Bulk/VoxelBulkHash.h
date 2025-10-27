// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct VOXELCORE_API FVoxelBulkHash
{
public:
	FVoxelBulkHash() = default;

	FString ToString() const;

	static FVoxelBulkHash Create(const TConstVoxelArrayView<uint8> Bytes);

public:
	FORCEINLINE bool IsNull() const
	{
		return
			Word0 == 0 &&
			Word1 == 0;
	}
	FORCEINLINE bool operator==(const FVoxelBulkHash& Other) const
	{
		return
			Word0 == Other.Word0 &&
			Word1 == Other.Word1;
	}
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FVoxelBulkHash& Hash)
	{
		Ar << Hash.Word0;
		Ar << Hash.Word1;
		return Ar;
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelBulkHash& Hash)
	{
		return uint32(Hash.Word0);
	}

public:
	template<typename T>
	requires
	(
		std::is_trivially_destructible_v<T> &&
		!TIsTArrayView<T>::Value
	)
	static FORCEINLINE FVoxelBulkHash Create(const TConstVoxelArrayView<T> Array)
	{
		return FVoxelBulkHash::Create(Array.template ReinterpretAs<uint8>());
	}

	template<typename T, typename AllocatorType>
	requires
	(
		std::is_trivially_destructible_v<T> &&
		!TIsTArrayView<T>::Value
	)
	static FORCEINLINE FVoxelBulkHash Create(const TVoxelArray<T, AllocatorType>& Array)
	{
		return FVoxelBulkHash::Create(Array.template View<uint8>());
	}

private:
	uint64 Word0 = 0;
	uint64 Word1 = 0;
};