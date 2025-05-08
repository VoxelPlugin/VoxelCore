// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "TransvoxelData.h"

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

const TConstArray<FCellIndices, 16> CellClassToCellIndices =
{
	FCellIndices(),
	FCellIndices({ 0, 1, 2 }),
	FCellIndices({ 0, 1, 2, 3, 4, 5 }),
	FCellIndices({ 0, 1, 2, 0, 2, 3 }),
	FCellIndices({ 0, 1, 4, 1, 3, 4, 1, 2, 3 }),
	FCellIndices({ 0, 1, 2, 0, 2, 3, 4, 5, 6 }),
	FCellIndices({ 0, 1, 2, 3, 4, 5, 6, 7, 8 }),
	FCellIndices({ 0, 1, 4, 1, 3, 4, 1, 2, 3, 5, 6, 7 }),
	FCellIndices({ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 }),
	FCellIndices({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }),
	FCellIndices({ 0, 4, 5, 0, 1, 4, 1, 3, 4, 1, 2, 3 }),
	FCellIndices({ 0, 5, 4, 0, 4, 1, 1, 4, 3, 1, 3, 2 }),
	FCellIndices({ 0, 4, 5, 0, 3, 4, 0, 1, 3, 1, 2, 3 }),
	FCellIndices({ 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 }),
	FCellIndices({ 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6 }),
	FCellIndices({ 0, 4, 5, 0, 3, 4, 0, 1, 3, 1, 2, 3, 6, 7, 8 })
};

template<uint64 Index>
constexpr uint64 MakeImpl()
{
	return 0;
}

template<uint64 Index, uint64 X, uint64... Ys>
constexpr uint64 MakeImpl()
{
	checkStatic(0 <= Index && Index < 16);
	return (X << (4 * Index)) | MakeImpl<Index + 1, Ys...>();
}

template<uint64... Args>
constexpr uint64 MakePackedCellClass()
{
	return MakeImpl<0, Args...>();
}

const TConstArray<uint64, 16> CellCodeToPackedCellClass =
{
	MakePackedCellClass<0,  1,  1,  3,  1,  3,  2,  4,  1,  2,  3,  4,  3,  4,  4,  3>(),
	MakePackedCellClass<1,  3,  2,  4,  2,  4,  6, 12,  2,  5,  5, 11,  5, 10,  7,  4>(),
	MakePackedCellClass<1,  2,  3,  4,  2,  5,  5, 10,  2,  6,  4, 12,  5,  7, 11,  4>(),
	MakePackedCellClass<3,  4,  4,  3,  5, 11,  7,  4,  5,  7, 10,  4,  8, 14, 14,  3>(),
	MakePackedCellClass<1,  2,  2,  5,  3,  4,  5, 11,  2,  6,  5,  7,  4, 12, 10,  4>(),
	MakePackedCellClass<3,  4,  5, 10,  4,  3,  7,  4,  5,  7,  8, 14, 11,  4, 14,  3>(),
	MakePackedCellClass<2,  6,  5,  7,  5,  7,  8, 14,  6,  9,  7, 15,  7, 15, 14, 13>(),
	MakePackedCellClass<4, 12, 11,  4, 10,  4, 14,  3,  7, 15, 14, 13, 14, 13,  2,  1>(),
	MakePackedCellClass<1,  2,  2,  5,  2,  5,  6,  7,  3,  5,  4, 10,  4, 11, 12,  4>(),
	MakePackedCellClass<2,  5,  6,  7,  6,  7,  9, 15,  5,  8,  7, 14,  7, 14, 15, 13>(),
	MakePackedCellClass<3,  5,  4, 11,  5,  8,  7, 14,  4,  7,  3,  4, 10, 14,  4,  3>(),
	MakePackedCellClass<4, 10, 12,  4,  7, 14, 15, 13, 11, 14,  4,  3, 14,  2, 13,  1>(),
	MakePackedCellClass<3,  5,  5,  8,  4, 10,  7, 14,  4,  7, 11, 14,  3,  4,  4,  3>(),
	MakePackedCellClass<4, 11,  7, 14, 12,  4, 15, 13, 10, 14, 14,  2,  4,  3, 13,  1>(),
	MakePackedCellClass<4,  7, 10, 14, 11, 14, 14,  2, 12, 15,  4, 13,  4, 13,  3,  1>(),
	MakePackedCellClass<3,  4,  4,  3,  4,  3, 13,  1,  4, 13,  3,  1,  3,  1,  1,  0>()
};

template<uint64... Args>
constexpr FCellVertices MakeVertexDatas()
{
	checkStatic(sizeof...(Args) <= 12);

	return FCellVertices{ MakeImpl<0, Args...>() | (uint64(sizeof...(Args)) << 48) };
}

const TConstArray<FCellVertices, 256> CellCodeToCellVertices =
{
	{},
	MakeVertexDatas<0, 4, 8>(),
	MakeVertexDatas<0, 9, 6>(),
	MakeVertexDatas<4, 8, 9, 6>(),
	MakeVertexDatas<4, 1, 10>(),
	MakeVertexDatas<8, 0, 1, 10>(),
	MakeVertexDatas<0, 9, 6, 4, 1, 10>(),
	MakeVertexDatas<1, 10, 8, 9, 6>(),
	MakeVertexDatas<6, 11, 1>(),
	MakeVertexDatas<0, 4, 8, 1, 6, 11>(),
	MakeVertexDatas<0, 9, 11, 1>(),
	MakeVertexDatas<4, 8, 9, 11, 1>(),
	MakeVertexDatas<4, 6, 11, 10>(),
	MakeVertexDatas<6, 11, 10, 8, 0>(),
	MakeVertexDatas<0, 9, 11, 10, 4>(),
	MakeVertexDatas<8, 9, 11, 10>(),
	MakeVertexDatas<8, 5, 2>(),
	MakeVertexDatas<0, 4, 5, 2>(),
	MakeVertexDatas<0, 9, 6, 8, 5, 2>(),
	MakeVertexDatas<9, 6, 4, 5, 2>(),
	MakeVertexDatas<4, 1, 10, 8, 5, 2>(),
	MakeVertexDatas<5, 2, 0, 1, 10>(),
	MakeVertexDatas<8, 5, 2, 0, 9, 6, 4, 1, 10>(),
	MakeVertexDatas<1, 10, 5, 2, 9, 6>(),
	MakeVertexDatas<1, 6, 11, 8, 5, 2>(),
	MakeVertexDatas<0, 4, 5, 2, 1, 6, 11>(),
	MakeVertexDatas<1, 0, 9, 11, 8, 5, 2>(),
	MakeVertexDatas<1, 11, 9, 2, 5, 4>(),
	MakeVertexDatas<4, 6, 11, 10, 8, 5, 2>(),
	MakeVertexDatas<6, 11, 10, 5, 2, 0>(),
	MakeVertexDatas<0, 9, 11, 10, 4, 8, 5, 2>(),
	MakeVertexDatas<2, 9, 11, 10, 5>(),
	MakeVertexDatas<9, 2, 7>(),
	MakeVertexDatas<0, 4, 8, 9, 2, 7>(),
	MakeVertexDatas<6, 0, 2, 7>(),
	MakeVertexDatas<2, 7, 6, 4, 8>(),
	MakeVertexDatas<4, 1, 10, 9, 2, 7>(),
	MakeVertexDatas<0, 1, 10, 8, 9, 2, 7>(),
	MakeVertexDatas<0, 2, 7, 6, 4, 1, 10>(),
	MakeVertexDatas<1, 10, 8, 2, 7, 6>(),
	MakeVertexDatas<1, 6, 11, 9, 2, 7>(),
	MakeVertexDatas<0, 4, 8, 1, 6, 11, 9, 2, 7>(),
	MakeVertexDatas<11, 1, 0, 2, 7>(),
	MakeVertexDatas<4, 8, 2, 7, 11, 1>(),
	MakeVertexDatas<4, 6, 11, 10, 9, 2, 7>(),
	MakeVertexDatas<6, 11, 10, 8, 0, 9, 2, 7>(),
	MakeVertexDatas<4, 10, 11, 7, 2, 0>(),
	MakeVertexDatas<7, 11, 10, 8, 2>(),
	MakeVertexDatas<9, 8, 5, 7>(),
	MakeVertexDatas<0, 4, 5, 7, 9>(),
	MakeVertexDatas<8, 5, 7, 6, 0>(),
	MakeVertexDatas<6, 4, 5, 7>(),
	MakeVertexDatas<9, 8, 5, 7, 4, 1, 10>(),
	MakeVertexDatas<10, 1, 0, 9, 7, 5>(),
	MakeVertexDatas<8, 5, 7, 6, 0, 4, 1, 10>(),
	MakeVertexDatas<10, 5, 7, 6, 1>(),
	MakeVertexDatas<9, 8, 5, 7, 1, 6, 11>(),
	MakeVertexDatas<0, 4, 5, 7, 9, 1, 6, 11>(),
	MakeVertexDatas<8, 5, 7, 11, 1, 0>(),
	MakeVertexDatas<1, 4, 5, 7, 11>(),
	MakeVertexDatas<9, 8, 5, 7, 4, 6, 11, 10>(),
	MakeVertexDatas<0, 6, 11, 10, 5, 7, 9>(),
	MakeVertexDatas<0, 8, 5, 7, 11, 10, 4>(),
	MakeVertexDatas<10, 5, 7, 11>(),
	MakeVertexDatas<10, 3, 5>(),
	MakeVertexDatas<0, 4, 8, 10, 3, 5>(),
	MakeVertexDatas<0, 9, 6, 10, 3, 5>(),
	MakeVertexDatas<4, 8, 9, 6, 10, 3, 5>(),
	MakeVertexDatas<4, 1, 3, 5>(),
	MakeVertexDatas<8, 0, 1, 3, 5>(),
	MakeVertexDatas<4, 1, 3, 5, 0, 9, 6>(),
	MakeVertexDatas<5, 3, 1, 6, 9, 8>(),
	MakeVertexDatas<6, 11, 1, 10, 3, 5>(),
	MakeVertexDatas<0, 4, 8, 1, 6, 11, 10, 3, 5>(),
	MakeVertexDatas<0, 9, 11, 1, 10, 3, 5>(),
	MakeVertexDatas<4, 8, 9, 11, 1, 10, 3, 5>(),
	MakeVertexDatas<3, 5, 4, 6, 11>(),
	MakeVertexDatas<0, 6, 11, 3, 5, 8>(),
	MakeVertexDatas<0, 9, 11, 3, 5, 4>(),
	MakeVertexDatas<5, 8, 9, 11, 3>(),
	MakeVertexDatas<8, 10, 3, 2>(),
	MakeVertexDatas<10, 3, 2, 0, 4>(),
	MakeVertexDatas<8, 10, 3, 2, 0, 9, 6>(),
	MakeVertexDatas<10, 3, 2, 9, 6, 4>(),
	MakeVertexDatas<4, 1, 3, 2, 8>(),
	MakeVertexDatas<0, 1, 3, 2>(),
	MakeVertexDatas<4, 1, 3, 2, 8, 0, 9, 6>(),
	MakeVertexDatas<6, 1, 3, 2, 9>(),
	MakeVertexDatas<8, 10, 3, 2, 1, 6, 11>(),
	MakeVertexDatas<10, 3, 2, 0, 4, 1, 6, 11>(),
	MakeVertexDatas<8, 10, 3, 2, 1, 0, 9, 11>(),
	MakeVertexDatas<4, 10, 3, 2, 9, 11, 1>(),
	MakeVertexDatas<8, 2, 3, 11, 6, 4>(),
	MakeVertexDatas<11, 3, 2, 0, 6>(),
	MakeVertexDatas<4, 0, 9, 11, 3, 2, 8>(),
	MakeVertexDatas<9, 11, 3, 2>(),
	MakeVertexDatas<9, 2, 7, 10, 3, 5>(),
	MakeVertexDatas<0, 4, 8, 9, 2, 7, 10, 3, 5>(),
	MakeVertexDatas<0, 2, 7, 6, 10, 3, 5>(),
	MakeVertexDatas<2, 7, 6, 4, 8, 10, 3, 5>(),
	MakeVertexDatas<1, 3, 5, 4, 9, 2, 7>(),
	MakeVertexDatas<8, 0, 1, 3, 5, 9, 2, 7>(),
	MakeVertexDatas<1, 3, 5, 4, 0, 2, 7, 6>(),
	MakeVertexDatas<8, 2, 7, 6, 1, 3, 5>(),
	MakeVertexDatas<1, 6, 11, 9, 2, 7, 10, 3, 5>(),
	MakeVertexDatas<0, 4, 8, 1, 6, 11, 9, 2, 7, 10, 3, 5>(),
	MakeVertexDatas<11, 1, 0, 2, 7, 10, 3, 5>(),
	MakeVertexDatas<1, 4, 8, 2, 7, 11, 10, 3, 5>(),
	MakeVertexDatas<3, 5, 4, 6, 11, 9, 2, 7>(),
	MakeVertexDatas<0, 6, 11, 3, 5, 8, 9, 2, 7>(),
	MakeVertexDatas<11, 3, 5, 4, 0, 2, 7>(),
	MakeVertexDatas<8, 2, 7, 11, 3, 5>(),
	MakeVertexDatas<7, 9, 8, 10, 3>(),
	MakeVertexDatas<3, 7, 9, 0, 4, 10>(),
	MakeVertexDatas<3, 10, 8, 0, 6, 7>(),
	MakeVertexDatas<3, 7, 6, 4, 10>(),
	MakeVertexDatas<4, 1, 3, 7, 9, 8>(),
	MakeVertexDatas<9, 0, 1, 3, 7>(),
	MakeVertexDatas<8, 4, 1, 3, 7, 6, 0>(),
	MakeVertexDatas<6, 1, 3, 7>(),
	MakeVertexDatas<7, 9, 8, 10, 3, 1, 6, 11>(),
	MakeVertexDatas<7, 9, 0, 4, 10, 3, 1, 6, 11>(),
	MakeVertexDatas<7, 11, 1, 0, 8, 10, 3>(),
	MakeVertexDatas<4, 10, 3, 7, 11, 1>(),
	MakeVertexDatas<3, 7, 9, 8, 4, 6, 11>(),
	MakeVertexDatas<0, 6, 11, 3, 7, 9>(),
	MakeVertexDatas<0, 8, 4, 11, 3, 7>(),
	MakeVertexDatas<11, 3, 7>(),
	MakeVertexDatas<11, 7, 3>(),
	MakeVertexDatas<0, 4, 8, 11, 7, 3>(),
	MakeVertexDatas<0, 9, 6, 11, 7, 3>(),
	MakeVertexDatas<4, 8, 9, 6, 11, 7, 3>(),
	MakeVertexDatas<4, 1, 10, 11, 7, 3>(),
	MakeVertexDatas<0, 1, 10, 8, 11, 7, 3>(),
	MakeVertexDatas<0, 9, 6, 4, 1, 10, 11, 7, 3>(),
	MakeVertexDatas<1, 10, 8, 9, 6, 11, 7, 3>(),
	MakeVertexDatas<6, 7, 3, 1>(),
	MakeVertexDatas<1, 6, 7, 3, 0, 4, 8>(),
	MakeVertexDatas<7, 3, 1, 0, 9>(),
	MakeVertexDatas<8, 9, 7, 3, 1, 4>(),
	MakeVertexDatas<10, 4, 6, 7, 3>(),
	MakeVertexDatas<7, 6, 0, 8, 10, 3>(),
	MakeVertexDatas<10, 4, 0, 9, 7, 3>(),
	MakeVertexDatas<3, 10, 8, 9, 7>(),
	MakeVertexDatas<8, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<0, 4, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<0, 9, 6, 8, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<9, 6, 4, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<4, 1, 10, 8, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<5, 2, 0, 1, 10, 11, 7, 3>(),
	MakeVertexDatas<0, 9, 6, 4, 1, 10, 8, 5, 2, 11, 7, 3>(),
	MakeVertexDatas<6, 1, 10, 5, 2, 9, 11, 7, 3>(),
	MakeVertexDatas<1, 6, 7, 3, 8, 5, 2>(),
	MakeVertexDatas<0, 4, 5, 2, 1, 6, 7, 3>(),
	MakeVertexDatas<7, 3, 1, 0, 9, 8, 5, 2>(),
	MakeVertexDatas<9, 7, 3, 1, 4, 5, 2>(),
	MakeVertexDatas<10, 4, 6, 7, 3, 8, 5, 2>(),
	MakeVertexDatas<10, 5, 2, 0, 6, 7, 3>(),
	MakeVertexDatas<4, 0, 9, 7, 3, 10, 8, 5, 2>(),
	MakeVertexDatas<10, 5, 2, 9, 7, 3>(),
	MakeVertexDatas<9, 2, 3, 11>(),
	MakeVertexDatas<9, 2, 3, 11, 0, 4, 8>(),
	MakeVertexDatas<6, 0, 2, 3, 11>(),
	MakeVertexDatas<4, 6, 11, 3, 2, 8>(),
	MakeVertexDatas<9, 2, 3, 11, 4, 1, 10>(),
	MakeVertexDatas<0, 1, 10, 8, 11, 9, 2, 3>(),
	MakeVertexDatas<6, 0, 2, 3, 11, 4, 1, 10>(),
	MakeVertexDatas<6, 1, 10, 8, 2, 3, 11>(),
	MakeVertexDatas<9, 2, 3, 1, 6>(),
	MakeVertexDatas<9, 2, 3, 1, 6, 0, 4, 8>(),
	MakeVertexDatas<0, 2, 3, 1>(),
	MakeVertexDatas<8, 2, 3, 1, 4>(),
	MakeVertexDatas<4, 6, 9, 2, 3, 10>(),
	MakeVertexDatas<6, 9, 2, 3, 10, 8, 0>(),
	MakeVertexDatas<4, 0, 2, 3, 10>(),
	MakeVertexDatas<8, 2, 3, 10>(),
	MakeVertexDatas<3, 11, 9, 8, 5>(),
	MakeVertexDatas<4, 5, 3, 11, 9, 0>(),
	MakeVertexDatas<8, 5, 3, 11, 6, 0>(),
	MakeVertexDatas<11, 6, 4, 5, 3>(),
	MakeVertexDatas<3, 11, 9, 8, 5, 4, 1, 10>(),
	MakeVertexDatas<5, 3, 11, 9, 0, 1, 10>(),
	MakeVertexDatas<3, 11, 6, 0, 8, 5, 4, 1, 10>(),
	MakeVertexDatas<6, 1, 10, 5, 3, 11>(),
	MakeVertexDatas<8, 9, 6, 1, 3, 5>(),
	MakeVertexDatas<9, 0, 4, 5, 3, 1, 6>(),
	MakeVertexDatas<5, 3, 1, 0, 8>(),
	MakeVertexDatas<4, 5, 3, 1>(),
	MakeVertexDatas<3, 10, 4, 6, 9, 8, 5>(),
	MakeVertexDatas<0, 6, 9, 10, 5, 3>(),
	MakeVertexDatas<0, 8, 5, 3, 10, 4>(),
	MakeVertexDatas<10, 5, 3>(),
	MakeVertexDatas<10, 11, 7, 5>(),
	MakeVertexDatas<11, 7, 5, 10, 0, 4, 8>(),
	MakeVertexDatas<11, 7, 5, 10, 0, 9, 6>(),
	MakeVertexDatas<6, 4, 8, 9, 10, 11, 7, 5>(),
	MakeVertexDatas<11, 7, 5, 4, 1>(),
	MakeVertexDatas<0, 1, 11, 7, 5, 8>(),
	MakeVertexDatas<11, 7, 5, 4, 1, 0, 9, 6>(),
	MakeVertexDatas<1, 11, 7, 5, 8, 9, 6>(),
	MakeVertexDatas<1, 6, 7, 5, 10>(),
	MakeVertexDatas<1, 6, 7, 5, 10, 0, 4, 8>(),
	MakeVertexDatas<5, 7, 9, 0, 1, 10>(),
	MakeVertexDatas<1, 4, 8, 9, 7, 5, 10>(),
	MakeVertexDatas<6, 7, 5, 4>(),
	MakeVertexDatas<0, 6, 7, 5, 8>(),
	MakeVertexDatas<9, 7, 5, 4, 0>(),
	MakeVertexDatas<9, 7, 5, 8>(),
	MakeVertexDatas<2, 8, 10, 11, 7>(),
	MakeVertexDatas<0, 2, 7, 11, 10, 4>(),
	MakeVertexDatas<2, 8, 10, 11, 7, 0, 9, 6>(),
	MakeVertexDatas<2, 9, 6, 4, 10, 11, 7>(),
	MakeVertexDatas<1, 11, 7, 2, 8, 4>(),
	MakeVertexDatas<7, 2, 0, 1, 11>(),
	MakeVertexDatas<2, 8, 4, 1, 11, 7, 6, 0, 9>(),
	MakeVertexDatas<1, 11, 7, 2, 9, 6>(),
	MakeVertexDatas<6, 7, 2, 8, 10, 1>(),
	MakeVertexDatas<10, 1, 6, 7, 2, 0, 4>(),
	MakeVertexDatas<7, 2, 8, 10, 1, 0, 9>(),
	MakeVertexDatas<4, 10, 1, 9, 7, 2>(),
	MakeVertexDatas<8, 4, 6, 7, 2>(),
	MakeVertexDatas<6, 7, 2, 0>(),
	MakeVertexDatas<4, 0, 9, 7, 2, 8>(),
	MakeVertexDatas<9, 7, 2>(),
	MakeVertexDatas<5, 10, 11, 9, 2>(),
	MakeVertexDatas<5, 10, 11, 9, 2, 0, 4, 8>(),
	MakeVertexDatas<0, 2, 5, 10, 11, 6>(),
	MakeVertexDatas<2, 5, 10, 11, 6, 4, 8>(),
	MakeVertexDatas<4, 5, 2, 9, 11, 1>(),
	MakeVertexDatas<5, 8, 0, 1, 11, 9, 2>(),
	MakeVertexDatas<11, 6, 0, 2, 5, 4, 1>(),
	MakeVertexDatas<1, 11, 6, 8, 2, 5>(),
	MakeVertexDatas<6, 9, 2, 5, 10, 1>(),
	MakeVertexDatas<5, 10, 1, 6, 9, 2, 0, 4, 8>(),
	MakeVertexDatas<10, 1, 0, 2, 5>(),
	MakeVertexDatas<1, 4, 8, 2, 5, 10>(),
	MakeVertexDatas<2, 5, 4, 6, 9>(),
	MakeVertexDatas<6, 9, 2, 5, 8, 0>(),
	MakeVertexDatas<0, 2, 5, 4>(),
	MakeVertexDatas<8, 2, 5>(),
	MakeVertexDatas<8, 10, 11, 9>(),
	MakeVertexDatas<4, 10, 11, 9, 0>(),
	MakeVertexDatas<0, 8, 10, 11, 6>(),
	MakeVertexDatas<4, 10, 11, 6>(),
	MakeVertexDatas<1, 11, 9, 8, 4>(),
	MakeVertexDatas<0, 1, 11, 9>(),
	MakeVertexDatas<8, 4, 1, 11, 6, 0>(),
	MakeVertexDatas<6, 1, 11>(),
	MakeVertexDatas<6, 9, 8, 10, 1>(),
	MakeVertexDatas<10, 1, 6, 9, 0, 4>(),
	MakeVertexDatas<8, 10, 1, 0>(),
	MakeVertexDatas<4, 10, 1>(),
	MakeVertexDatas<4, 6, 9, 8>(),
	MakeVertexDatas<0, 6, 9>(),
	MakeVertexDatas<0, 8, 4>(),
	{}
};

}