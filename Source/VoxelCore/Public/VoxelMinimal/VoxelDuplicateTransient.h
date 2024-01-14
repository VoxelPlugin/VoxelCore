// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename T>
struct TVoxelDuplicateTransient
{
	T Data{};

	TVoxelDuplicateTransient() = default;
	FORCEINLINE TVoxelDuplicateTransient(TVoxelDuplicateTransient&&)
	{
	}
	FORCEINLINE TVoxelDuplicateTransient(const TVoxelDuplicateTransient&)
	{
	}
	FORCEINLINE TVoxelDuplicateTransient& operator=(TVoxelDuplicateTransient&&)
	{
		return *this;
	}
	FORCEINLINE TVoxelDuplicateTransient& operator=(const TVoxelDuplicateTransient&)
	{
		return *this;
	}

	FORCEINLINE TVoxelDuplicateTransient& operator=(T&& NewData)
	{
		Data = MoveTemp(NewData);
		return *this;
	}
	FORCEINLINE TVoxelDuplicateTransient& operator=(const T& NewData)
	{
		Data = NewData;
		return *this;
	}

	FORCEINLINE T& Get()
	{
		return Data;
	}
	FORCEINLINE const T& Get() const
	{
		return Data;
	}

	FORCEINLINE T& operator*()
	{
		return Data;
	}
	FORCEINLINE const T& operator*() const
	{
		return Data;
	}

	FORCEINLINE T* operator->()
	{
		return &Data;
	}
	FORCEINLINE const T* operator->() const
	{
		return &Data;
	}

	template<typename IndexType>
	FORCEINLINE auto operator[](IndexType Index) -> decltype(auto)
	{
		return Data[Index];
	}
	template<typename IndexType>
	FORCEINLINE auto operator[](IndexType Index) const -> decltype(auto)
	{
		return Data[Index];
	}
};