// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

struct FVoxelGuid
{
public:
	uint32 A = 0;
	uint32 B = 0;
	uint32 C = 0;
	uint32 D = 0;

	FVoxelGuid() = default;

	template<
		char CharA0, char CharA1, char CharA2, char CharA3, char CharA4, char CharA5, char CharA6, char CharA7,
		char CharB0, char CharB1, char CharB2, char CharB3, char CharB4, char CharB5, char CharB6, char CharB7,
		char CharC0, char CharC1, char CharC2, char CharC3, char CharC4, char CharC5, char CharC6, char CharC7,
		char CharD0, char CharD1, char CharD2, char CharD3, char CharD4, char CharD5, char CharD6, char CharD7,
		char ZeroChar>
	static constexpr FVoxelGuid Make()
	{
		checkStatic(ZeroChar == 0);

		FVoxelGuid Guid;
		Guid.A = CharsToInt<CharA0, CharA1, CharA2, CharA3, CharA4, CharA5, CharA6, CharA7>();
		Guid.B = CharsToInt<CharB0, CharB1, CharB2, CharB3, CharB4, CharB5, CharB6, CharB7>();
		Guid.C = CharsToInt<CharC0, CharC1, CharC2, CharC3, CharC4, CharC5, CharC6, CharC7>();
		Guid.D = CharsToInt<CharD0, CharD1, CharD2, CharD3, CharD4, CharD5, CharD6, CharD7>();
		return Guid;
	}

	operator FGuid() const
	{
		return FGuid(A, B, C, D);
	}

private:
	template<char Char>
	static constexpr typename TEnableIf<'0' <= Char && Char <= '9', uint32>::Type CharToInt()
	{
		return Char - '0';
	}
	template<char Char>
	static constexpr typename TEnableIf<'A' <= Char && Char <= 'F', uint32>::Type CharToInt()
	{
		return 10 + Char - 'A';
	}
	template<char Char0, char Char1, char Char2, char Char3, char Char4, char Char5, char Char6, char Char7>
	static constexpr uint32 CharsToInt()
	{
		return
		{
			(CharToInt<Char0>() << 0) |
			(CharToInt<Char1>() << 4) |
			(CharToInt<Char2>() << 8) |
			(CharToInt<Char3>() << 12) |
			(CharToInt<Char4>() << 16) |
			(CharToInt<Char5>() << 20) |
			(CharToInt<Char6>() << 24) |
			(CharToInt<Char7>() << 28)
		};
	}
};

#define MAKE_VOXEL_GUID(String) FVoxelGuid::Make< \
	String[0], String[1], String[2], String[3], String[4], String[5], String[6], String[7], \
	String[8], String[9], String[10], String[11], String[12], String[13], String[14], String[15], \
	String[16], String[17], String[18], String[19], String[20], String[21], String[22], String[23], \
	String[24], String[25], String[26], String[27], String[28], String[29], String[30], String[31], String[32]>()