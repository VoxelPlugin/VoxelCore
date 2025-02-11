// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class FMaterialCompiler;
class UMaterialFunction;
class UMaterialExpression;

namespace FVoxelUtilities
{
#if WITH_EDITOR
	VOXELCORE_API int32 ZeroDerivative(
		FMaterialCompiler& Compiler,
		int32 Index);

	VOXELCORE_API UMaterialExpression& CreateMaterialExpression(
		UObject& Outer,
		TSubclassOf<UMaterialExpression> ExpressionClass);

	template<typename T> requires(std::derived_from<T, UMaterialExpression>)
	T& CreateMaterialExpression(UObject& Outer)
	{
		return *CastChecked<T>(&FVoxelUtilities::CreateMaterialExpression(Outer, T::StaticClass()));
	}
#endif
};