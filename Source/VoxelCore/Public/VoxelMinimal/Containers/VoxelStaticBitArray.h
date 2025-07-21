// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Containers/VoxelBitArrayView.h"

template<int32 Size>
requires (Size % FConstVoxelBitArrayView::NumBitsPerWord == 0)
class TVoxelStaticBitArray
{
public:
	TVoxelStaticBitArray() = default;
	FORCEINLINE explicit TVoxelStaticBitArray(EForceInit)
	{
		Memzero();
	}

public:
	FORCEINLINE static constexpr int32 Num()
	{
		return Size;
	}
	FORCEINLINE static constexpr int32 NumWords()
	{
		checkStatic(Size % FConstVoxelBitArrayView::NumBitsPerWord == 0);
		return Size / FConstVoxelBitArrayView::NumBitsPerWord;
	}

public:
	FORCEINLINE void Memzero()
	{
		Words.Memzero();
	}
	FORCEINLINE FVoxelSetBitIterator IterateSetBits() const
	{
		return View().IterateSetBits();
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue)
	{
		return View().AtomicSet_ReturnOld(Index, bValue);
	}
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue)
	{
		View().AtomicSet(Index, bValue);
	}

public:
	FORCEINLINE FVoxelBitArrayView View()
	{
		return FVoxelBitArrayView(Words.GetData(), Num());
	}
	FORCEINLINE FConstVoxelBitArrayView View() const
	{
		return FConstVoxelBitArrayView(Words.GetData(), Num());
	}

	FORCEINLINE FVoxelBitReference operator[](const int32 Index)
	{
		return View()[Index];
	}
	FORCEINLINE FVoxelConstBitReference operator[](const int32 Index) const
	{
		return View()[Index];
	}

private:
	checkStatic(Size % FConstVoxelBitArrayView::NumBitsPerWord == 0);
	TVoxelStaticArray<uint64, Size / FConstVoxelBitArrayView::NumBitsPerWord> Words{ NoInit };
};
checkStatic(std::is_trivially_destructible_v<TVoxelStaticBitArray<64>>);