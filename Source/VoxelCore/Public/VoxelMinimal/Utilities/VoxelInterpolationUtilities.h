// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	/**
	 * Y
	 * ^ C - D
	 * | |   |
	 * | A - B
	 *  -----> X
	 */
	template<typename T, typename U = float>
	FORCEINLINE T BilinearInterpolation(T A, T B, T C, T D, U X, U Y)
	{
		T AB = FMath::Lerp(A, B, X);
		T CD = FMath::Lerp(C, D, X);
		return FMath::Lerp(AB, CD, Y);
	}

	/**
	 * Y
	 * ^ C - D
	 * | |   |
	 * | A - B
	 * 0-----> X
	 * Y
	 * ^ G - H
	 * | |   |
	 * | E - F
	 * 1-----> X
	 */
	template<typename T, typename U = float>
	FORCEINLINE T TrilinearInterpolation(
		T A, T B, T C, T D,
		T E, T F, T G, T H,
		U X, U Y, U Z)
	{
		const T ABCD = BilinearInterpolation<T, U>(A, B, C, D, X, Y);
		const T EFGH = BilinearInterpolation<T, U>(E, F, G, H, X, Y);
		return FMath::Lerp(ABCD, EFGH, Z);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// H00
	FORCEINLINE float HermiteP0(const float T)
	{
		return (1 + 2 * T) * FMath::Square(1 - T);
	}
	// H10
	FORCEINLINE float HermiteD0(const float T)
	{
		return T * FMath::Square(1 - T);
	}

	// H01
	FORCEINLINE float HermiteP1(const float T)
	{
		return FMath::Square(T) * (3 - 2 * T);
	}
	// H11
	FORCEINLINE float HermiteD1(const float T)
	{
		return FMath::Square(T) * (T - 1);
	}
}