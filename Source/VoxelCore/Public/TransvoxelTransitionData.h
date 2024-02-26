// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
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

namespace Voxel::Transvoxel::Transition
{

/*
 * Edge index:
 * High res X:
 * 0 - 1: 0
 * 1 - 2: 1
 *
 * High res Y:
 * 0 - 3: 2
 * 3 - 6: 3
 *
 * Low res XY:
 * 9 - A: 4
 * 9 - B: 5
 */

struct FCellClass
{
	uint8 Index : 7;
	uint8 bIsInverted : 1;
};

struct FVertexData
{
	uint8 IndexA : 4;
	uint8 EdgeIndex : 4;

	constexpr FVertexData()
		: IndexA(0)
		, EdgeIndex(0)
	{
	}
	constexpr FVertexData(const uint8 IndexA, const uint8 EdgeIndex)
		: IndexA(IndexA)
		, EdgeIndex(EdgeIndex)
	{
	}

	FORCEINLINE uint8 GetIndexB() const
	{
		if (IndexA < 9)
		{
			checkVoxelSlow(0 <= EdgeIndex && EdgeIndex < 4);
			return IndexA + (EdgeIndex < 2 ? 1 : 3);
		}
		else
		{
			checkVoxelSlow(EdgeIndex == 4 || EdgeIndex == 5);
			return IndexA + (EdgeIndex == 4 ? 1 : 2);
		}
	}
};
using FVertexDatas = TConstArray<FVertexData, 12>;

struct FTransitionCellData
{
	uint8 NumTriangles = 0;
	uint8 NumVertices = 0;
	uint8 Indices[36] = { 0 };

	constexpr FTransitionCellData() = default;

	template<std::size_t Num>
	constexpr explicit FTransitionCellData(const uint8 (&Array)[Num])
		: FTransitionCellData()
	{
		checkStatic(Num % 3 == 0);
		NumTriangles = Num / 3;

		uint8 MaxVertex = 0;
		for (int32 Index = 0; Index < Num; Index++)
		{
			MaxVertex = FMath::Max(MaxVertex, Array[Index]);
			Indices[Index] = Array[Index];
		}
		NumVertices = MaxVertex + 1;
	}
};

extern VOXELCORE_API const TConstArray<FCellClass, 512> CellCodeToCellClass;
extern VOXELCORE_API const TConstArray<FVertexDatas, 512> CellCodeToVertexDatas;
extern VOXELCORE_API const TConstArray<FTransitionCellData, 56> CellClassToTransitionCellData;

}