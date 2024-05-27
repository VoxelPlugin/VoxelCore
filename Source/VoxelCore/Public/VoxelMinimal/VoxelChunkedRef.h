// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"

struct FVoxelChunkedRefAllocation
{
	TVoxelChunkedArray<uint8>* const ByteArrayPtr;

	FORCEINLINE explicit FVoxelChunkedRefAllocation(TVoxelChunkedArray<uint8>& ByteArray)
		: ByteArrayPtr(&ByteArray)
	{
	}
};

FORCEINLINE FVoxelChunkedRefAllocation AllocateChunkedRef(TVoxelChunkedArray<uint8>& ByteArray)
{
	return FVoxelChunkedRefAllocation(ByteArray);
}

template<typename T>
class TVoxelChunkedRef
{
public:
	checkStatic(TIsTriviallyDestructible<T>::Value);

	FORCEINLINE TVoxelChunkedRef(
		TVoxelChunkedArray<uint8>& ByteArray,
		const int32 ArrayIndex)
		: ByteArray(ByteArray)
		, ByteIndex(ArrayIndex)
	{
	}
	FORCEINLINE TVoxelChunkedRef(const FVoxelChunkedRefAllocation Allocation)
		: TVoxelChunkedRef(
			*Allocation.ByteArrayPtr,
			Allocation.ByteArrayPtr->AddUninitialized(sizeof(T)))
	{
	}
	FORCEINLINE ~TVoxelChunkedRef()
	{
		ByteArray.CopyFrom(ByteIndex, MakeByteVoxelArrayView(Value));
	}

	FORCEINLINE T& operator*()
	{
		return Value;
	}
	FORCEINLINE const T& operator*() const
	{
		return Value;
	}

	FORCEINLINE T* operator->()
	{
		return &Value;
	}
	FORCEINLINE const T* operator->() const
	{
		return &Value;
	}

private:
	TVoxelChunkedArray<uint8>& ByteArray;
	const int32 ByteIndex;
	T Value = FVoxelUtilities::MakeSafe<T>();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelChunkedArrayRefAllocation
{
	TVoxelChunkedArray<uint8>* const ByteArrayPtr;
	const int32 Num;

	FORCEINLINE explicit FVoxelChunkedArrayRefAllocation(
		TVoxelChunkedArray<uint8>& Array,
		const int32 Num)
		: ByteArrayPtr(&Array)
		, Num(Num)
	{
	}
};

FORCEINLINE FVoxelChunkedArrayRefAllocation AllocateChunkedArrayRef(
	TVoxelChunkedArray<uint8>& ByteArray,
	const int32 Num)
{
	return FVoxelChunkedArrayRefAllocation(ByteArray, Num);
}

template<typename T>
class TVoxelChunkedArrayRef
{
public:
	checkStatic(TIsTriviallyDestructible<T>::Value);

	FORCEINLINE TVoxelChunkedArrayRef(
		TVoxelChunkedArray<uint8>& ByteArray,
		const int32 Num,
		const int32 ByteIndex)
		: ByteArray(ByteArray)
		, ByteIndex(ByteIndex)
	{
		FVoxelUtilities::SetNumFast(Values, Num);

		for (T& Value : Values)
		{
			Value = FVoxelUtilities::MakeSafe<T>();
		}
	}
	FORCEINLINE TVoxelChunkedArrayRef(const FVoxelChunkedArrayRefAllocation Allocation)
		: TVoxelChunkedArrayRef(
			*Allocation.ByteArrayPtr,
			Allocation.Num,
			Allocation.ByteArrayPtr->AddUninitialized(sizeof(T) * Allocation.Num))
	{
	}
	FORCEINLINE ~TVoxelChunkedArrayRef()
	{
		ByteArray.CopyFrom(ByteIndex, MakeByteVoxelArrayView(Values));
	}

	FORCEINLINE TVoxelArrayView<T> operator*()
	{
		return Values;
	}
	FORCEINLINE TConstVoxelArrayView<T> operator*() const
	{
		return Values;
	}

	FORCEINLINE T& operator[](const int32 Index)
	{
		return Values[Index];
	}
	FORCEINLINE const T& operator[](const int32 Index) const
	{
		return Values[Index];
	}

public:
	FORCEINLINE auto begin() -> decltype(auto)
	{
		return Values.begin();
	}
	FORCEINLINE auto end() -> decltype(auto)
	{
		return Values.end();
	}

	FORCEINLINE auto begin() const -> decltype(auto)
	{
		return Values.begin();
	}
	FORCEINLINE auto end() const -> decltype(auto)
	{
		return Values.end();
	}

private:
	TVoxelChunkedArray<uint8>& ByteArray;
	const int32 ByteIndex;
	TVoxelArray<T> Values;
};