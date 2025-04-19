// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

// See https://bertdobbelaere.github.io/sorting_networks_extended.html

template<int32 Size>
struct TVoxelSortNetwork;

template<>
struct TVoxelSortNetwork<2>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 1);
	}
};

template<>
struct TVoxelSortNetwork<3>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 2);
		Swap(0, 1);
		Swap(1, 2);
	}
};

template<>
struct TVoxelSortNetwork<4>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 2); Swap(1, 3);
		Swap(0, 1); Swap(2, 3);
		Swap(1, 2);
	}
};

template<>
struct TVoxelSortNetwork<5>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 3); Swap(1, 4);
		Swap(0, 2); Swap(1, 3);
		Swap(0, 1); Swap(2, 4);
		Swap(1, 2); Swap(3, 4);
		Swap(2, 3);
	}
};

template<>
struct TVoxelSortNetwork<6>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 5); Swap(1, 3); Swap(2, 4);
		Swap(1, 2); Swap(3, 4);
		Swap(0, 3); Swap(2, 5);
		Swap(0, 1); Swap(2, 3); Swap(4, 5);
		Swap(1, 2); Swap(3, 4);
	}
};

template<>
struct TVoxelSortNetwork<7>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 6); Swap(2, 3); Swap(4, 5);
		Swap(0, 2); Swap(1, 4); Swap(3, 6);
		Swap(0, 1); Swap(2, 5); Swap(3, 4);
		Swap(1, 2); Swap(4, 6);
		Swap(2, 3); Swap(4, 5);
		Swap(1, 2); Swap(3, 4); Swap(5, 6);
	}
};

template<>
struct TVoxelSortNetwork<8>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 2); Swap(1, 3); Swap(4, 6); Swap(5, 7);
		Swap(0, 4); Swap(1, 5); Swap(2, 6); Swap(3, 7);
		Swap(0, 1); Swap(2, 3); Swap(4, 5); Swap(6, 7);
		Swap(2, 4); Swap(3, 5);
		Swap(1, 4); Swap(3, 6);
		Swap(1, 2); Swap(3, 4); Swap(5, 6);
	}
};

template<>
struct TVoxelSortNetwork<9>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 3); Swap(1, 7); Swap(2, 5); Swap(4, 8);
		Swap(0, 7); Swap(2, 4); Swap(3, 8); Swap(5, 6);
		Swap(0, 2); Swap(1, 3); Swap(4, 5); Swap(7, 8);
		Swap(1, 4); Swap(3, 6); Swap(5, 7);
		Swap(0, 1); Swap(2, 4); Swap(3, 5); Swap(6, 8);
		Swap(2, 3); Swap(4, 5); Swap(6, 7);
		Swap(1, 2); Swap(3, 4); Swap(5, 6);
	}
};

template<>
struct TVoxelSortNetwork<10>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 1); Swap(2, 5); Swap(3, 6); Swap(4, 7); Swap(8, 9);
		Swap(0, 6); Swap(1, 8); Swap(2, 4); Swap(3, 9); Swap(5, 7);
		Swap(0, 2); Swap(1, 3); Swap(4, 5); Swap(6, 8); Swap(7, 9);
		Swap(0, 1); Swap(2, 7); Swap(3, 5); Swap(4, 6); Swap(8, 9);
		Swap(1, 2); Swap(3, 4); Swap(5, 6); Swap(7, 8);
		Swap(1, 3); Swap(2, 4); Swap(5, 7); Swap(6, 8);
		Swap(2, 3); Swap(4, 5); Swap(6, 7);
	}
};

template<>
struct TVoxelSortNetwork<11>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 9); Swap(1, 6); Swap(2, 4); Swap(3, 7); Swap(5, 8);
		Swap(0, 1); Swap(3, 5); Swap(4, 10); Swap(6, 9); Swap(7, 8);
		Swap(1, 3); Swap(2, 5); Swap(4, 7); Swap(8, 10);
		Swap(0, 4); Swap(1, 2); Swap(3, 7); Swap(5, 9); Swap(6, 8);
		Swap(0, 1); Swap(2, 6); Swap(4, 5); Swap(7, 8); Swap(9, 10);
		Swap(2, 4); Swap(3, 6); Swap(5, 7); Swap(8, 9);
		Swap(1, 2); Swap(3, 4); Swap(5, 6); Swap(7, 8);
		Swap(2, 3); Swap(4, 5); Swap(6, 7);
	}
};

template<>
struct TVoxelSortNetwork<12>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 8); Swap(1, 7); Swap(2, 6); Swap(3, 11); Swap(4, 10); Swap(5, 9);
		Swap(0, 1); Swap(2, 5); Swap(3, 4); Swap(6, 9); Swap(7, 8); Swap(10, 11);
		Swap(0, 2); Swap(1, 6); Swap(5, 10); Swap(9, 11);
		Swap(0, 3); Swap(1, 2); Swap(4, 6); Swap(5, 7); Swap(8, 11); Swap(9, 10);
		Swap(1, 4); Swap(3, 5); Swap(6, 8); Swap(7, 10);
		Swap(1, 3); Swap(2, 5); Swap(6, 9); Swap(8, 10);
		Swap(2, 3); Swap(4, 5); Swap(6, 7); Swap(8, 9);
		Swap(4, 6); Swap(5, 7);
		Swap(3, 4); Swap(5, 6); Swap(7, 8);
	}
};

template<>
struct TVoxelSortNetwork<13>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 12); Swap(1, 10); Swap(2, 9); Swap(3, 7); Swap(5, 11); Swap(6, 8);
		Swap(1, 6); Swap(2, 3); Swap(4, 11); Swap(7, 9); Swap(8, 10);
		Swap(0, 4); Swap(1, 2); Swap(3, 6); Swap(7, 8); Swap(9, 10); Swap(11, 12);
		Swap(4, 6); Swap(5, 9); Swap(8, 11); Swap(10, 12);
		Swap(0, 5); Swap(3, 8); Swap(4, 7); Swap(6, 11); Swap(9, 10);
		Swap(0, 1); Swap(2, 5); Swap(6, 9); Swap(7, 8); Swap(10, 11);
		Swap(1, 3); Swap(2, 4); Swap(5, 6); Swap(9, 10);
		Swap(1, 2); Swap(3, 4); Swap(5, 7); Swap(6, 8);
		Swap(2, 3); Swap(4, 5); Swap(6, 7); Swap(8, 9);
		Swap(3, 4); Swap(5, 6);
	}
};

template<>
struct TVoxelSortNetwork<14>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 1); Swap(2, 3); Swap(4, 5); Swap(6, 7); Swap(8, 9); Swap(10, 11); Swap(12, 13);
		Swap(0, 2); Swap(1, 3); Swap(4, 8); Swap(5, 9); Swap(10, 12); Swap(11, 13);
		Swap(0, 4); Swap(1, 2); Swap(3, 7); Swap(5, 8); Swap(6, 10); Swap(9, 13); Swap(11, 12);
		Swap(0, 6); Swap(1, 5); Swap(3, 9); Swap(4, 10); Swap(7, 13); Swap(8, 12);
		Swap(2, 10); Swap(3, 11); Swap(4, 6); Swap(7, 9);
		Swap(1, 3); Swap(2, 8); Swap(5, 11); Swap(6, 7); Swap(10, 12);
		Swap(1, 4); Swap(2, 6); Swap(3, 5); Swap(7, 11); Swap(8, 10); Swap(9, 12);
		Swap(2, 4); Swap(3, 6); Swap(5, 8); Swap(7, 10); Swap(9, 11);
		Swap(3, 4); Swap(5, 6); Swap(7, 8); Swap(9, 10);
		Swap(6, 7);
	}
};

template<>
struct TVoxelSortNetwork<15>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(1, 2); Swap(3, 10); Swap(4, 14); Swap(5, 8); Swap(6, 13); Swap(7, 12); Swap(9, 11);
		Swap(0, 14); Swap(1, 5); Swap(2, 8); Swap(3, 7); Swap(6, 9); Swap(10, 12); Swap(11, 13);
		Swap(0, 7); Swap(1, 6); Swap(2, 9); Swap(4, 10); Swap(5, 11); Swap(8, 13); Swap(12, 14);
		Swap(0, 6); Swap(2, 4); Swap(3, 5); Swap(7, 11); Swap(8, 10); Swap(9, 12); Swap(13, 14);
		Swap(0, 3); Swap(1, 2); Swap(4, 7); Swap(5, 9); Swap(6, 8); Swap(10, 11); Swap(12, 13);
		Swap(0, 1); Swap(2, 3); Swap(4, 6); Swap(7, 9); Swap(10, 12); Swap(11, 13);
		Swap(1, 2); Swap(3, 5); Swap(8, 10); Swap(11, 12);
		Swap(3, 4); Swap(5, 6); Swap(7, 8); Swap(9, 10);
		Swap(2, 3); Swap(4, 5); Swap(6, 7); Swap(8, 9); Swap(10, 11);
		Swap(5, 6); Swap(7, 8);
	}
};

template<>
struct TVoxelSortNetwork<16>
{
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(int32, int32)>
	static void Apply(LambdaType&& Swap)
	{
		Swap(0, 13); Swap(1, 12); Swap(2, 15); Swap(3, 14); Swap(4, 8); Swap(5, 6); Swap(7, 11); Swap(9, 10);
		Swap(0, 5); Swap(1, 7); Swap(2, 9); Swap(3, 4); Swap(6, 13); Swap(8, 14); Swap(10, 15); Swap(11, 12);
		Swap(0, 1); Swap(2, 3); Swap(4, 5); Swap(6, 8); Swap(7, 9); Swap(10, 11); Swap(12, 13); Swap(14, 15);
		Swap(0, 2); Swap(1, 3); Swap(4, 10); Swap(5, 11); Swap(6, 7); Swap(8, 9); Swap(12, 14); Swap(13, 15);
		Swap(1, 2); Swap(3, 12); Swap(4, 6); Swap(5, 7); Swap(8, 10); Swap(9, 11); Swap(13, 14);
		Swap(1, 4); Swap(2, 6); Swap(5, 8); Swap(7, 10); Swap(9, 13); Swap(11, 14);
		Swap(2, 4); Swap(3, 6); Swap(9, 12); Swap(11, 13);
		Swap(3, 5); Swap(6, 8); Swap(7, 9); Swap(10, 12);
		Swap(3, 4); Swap(5, 6); Swap(7, 8); Swap(9, 10); Swap(11, 12);
		Swap(6, 7); Swap(8, 9);
	}
};