// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

//================================================================================
//
// The Transvoxel Algorithm look-up tables
//
// Copyright 2009 by Eric Lengyel
//
// The following data originates from Eric Lengyel's Transvoxel Algorithm.
// http://transvoxel.org/
//
// The data in this file may be freely used in implementations of the Transvoxel
// Algorithm. If you do use this data, or any transformation of it, in your own
// projects, commercial or otherwise, please give credit by indicating in your
// source code that the data is part of the author's implementation of the
// Transvoxel Algorithm and that it came from the web address given above.
// (Simply copying and pasting the two lines of the previous paragraph would be
// perfect.) If you distribute a commercial product with source code included,
// then the credit in the source code is required.
//
// If you distribute any kind of product that uses this data, a credit visible to
// the end-user would be appreciated, but it is not required. However, you may
// not claim that the entire implementation of the Transvoxel Algorithm is your
// own if you use the data in this file or any transformation of it.
//
// The format of the data in this file is described in the dissertation "Voxel-
// Based TerrainObject for Real-Time Virtual Simulations", available at the web page
// given above. References to sections and figures below pertain to that paper.
//
// The contents of this file are protected by copyright and may not be publicly
// reproduced without permission.
//
//================================================================================

namespace Voxel::Transvoxel
{

struct FCellIndices
{
	uint64 Data = 0;

	FCellIndices() = default;

	template<std::size_t Num>
	constexpr explicit FCellIndices(const uint8 (&Array)[Num])
		: FCellIndices()
	{
		checkStatic(Num <= 15);
		checkStatic(Num % 3 == 0);
		Data |= uint64(Num / 3) << 60;

		for (int32 Index = 0; Index < Num; Index++)
		{
			Data |= uint64(Array[Index]) << (4 * Index);
		}
	}

	FORCEINLINE int32 NumTriangles() const
	{
		return int32(Data >> 60);
	}
	FORCEINLINE int32 GetIndex(const int32 Index) const
	{
		checkVoxelSlow(0 <= Index && Index < 3 * NumTriangles());
		return int32((Data >> (4 * Index)) & 0xF);
	}
};
checkStatic(sizeof(FCellIndices) == 8);

/**
 * Packed indices, one per edge
 *
 *	Index =
 *		1 * bool(IndexA & (1 << ((EdgeIndex + 1) % 3))) +
 *		2 * bool(IndexA & (1 << ((EdgeIndex + 2) % 3))) +
 *		4 * EdgeIndex;
 *
 *	EdgeIndex = Index / 4;
 *	IndexA = ((1 << ((EdgeIndex + 1) % 3)) * (Index % 4)) % 7;
 *
 * X:
 *  0: 0 - 1
 *  1: 2 - 3
 *  2: 4 - 5
 *  3: 6 - 7
 *
 * Y:
 *  4: 0 - 2
 *  5: 4 - 6
 *  6: 1 - 3
 *  7: 5 - 7
 *
 * Z:
 *  8: 0 - 4
 *  9: 1 - 5
 * 10: 2 - 6
 * 11: 3 - 7
 */

constexpr uint64 CacheDirectionLookup =
	(uint64(0b110) << 0) |
	(uint64(0b100) << 4) |
	(uint64(0b010) << 8) |
	(uint64(0b000) << 12) |
	(uint64(0b101) << 16) |
	(uint64(0b001) << 20) |
	(uint64(0b100) << 24) |
	(uint64(0b000) << 28) |
	(uint64(0b011) << 32) |
	(uint64(0b010) << 36) |
	(uint64(0b001) << 40) |
	(uint64(0b000) << 44);

struct FVertexData
{
	uint8 EdgeIndex;
	uint8 IndexA;
	uint8 IndexB;
	uint8 CacheDirection;

	FORCEINLINE explicit FVertexData(const int32 Index)
		: EdgeIndex(Index / 4)
		, IndexA(int32((0x321051406420 >> (4 * Index)) & 0xF))
		, IndexB(IndexA + (1 << EdgeIndex))
		, CacheDirection(uint8((CacheDirectionLookup >> (4 * Index)) & 0xF))
	{
	}
};
checkStatic(sizeof(FVertexData) == 4);

struct FCellVertices
{
	uint64 Data = 0;

	FORCEINLINE int32 NumVertices() const
	{
		return int32(Data >> 48);
	}
	FORCEINLINE FVertexData GetVertexData(const int32 Index) const
	{
		checkVoxelSlow(0 <= Index && Index < NumVertices());
		return FVertexData((Data >> (4 * Index)) & 0xF);
	}
};
checkStatic(sizeof(FCellVertices) == 8);

template<typename T, int32 Size>
struct alignas(128) TConstArray
{
public:
	constexpr TConstArray() = default;
	constexpr TConstArray(const std::initializer_list<T> Array)
	{
		int32 Index = 0;
		for (const T& Element : Array)
		{
			Data[Index++] = Element;
		}
	}

	FORCEINLINE const T& operator[](int32 Index) const
	{
		checkVoxelSlow(0 <= Index && Index < Size);
		return Data[Index];
	}

private:
	T Data[Size];

	TConstArray(const TConstArray&) = default;
	TConstArray& operator=(const TConstArray&) = default;

	template<typename, int32>
	friend struct TConstArray;
};

extern VOXELCORE_API const TConstArray<FCellIndices, 16> CellClassToCellIndices;
extern VOXELCORE_API const TConstArray<uint64, 16> CellCodeToPackedCellClass;
extern VOXELCORE_API const TConstArray<FCellVertices, 256> CellCodeToCellVertices;

FORCEINLINE int32 GetCellClass(const int32 CellCode)
{
	checkVoxelSlow(0 <= CellCode && CellCode < 256);
	const uint64 PackedCellClass = CellCodeToPackedCellClass[CellCode / 16];
	return FVoxelUtilities::ReadBits(PackedCellClass, 4 * (CellCode % 16), 4);
}

checkStatic(sizeof(CellCodeToPackedCellClass) == 128);
checkStatic(sizeof(CellClassToCellIndices) == 128);
checkStatic(sizeof(CellCodeToCellVertices) == 2048);

}