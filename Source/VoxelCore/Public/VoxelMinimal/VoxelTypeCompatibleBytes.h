// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename Type>
struct TVoxelTypeCompatibleBytes
{
public:
	TVoxelTypeCompatibleBytes() = default;
	~TVoxelTypeCompatibleBytes() = default;

	UE_NONCOPYABLE(TVoxelTypeCompatibleBytes);

	FORCEINLINE Type& GetValue()
	{
		return *reinterpret_cast<Type*>(Data);
	}
	FORCEINLINE const Type& GetValue() const
	{
		return *reinterpret_cast<const Type*>(Data);
	}

private:
	alignas(Type) uint8 Data[sizeof(Type)];
};

template<typename Type>
FORCEINLINE void* operator new(const size_t Size, TVoxelTypeCompatibleBytes<Type>& Bytes)
{
	checkVoxelSlow(Size == sizeof(Type));
	return &Bytes.GetValue();
}