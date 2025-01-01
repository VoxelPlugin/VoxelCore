// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class FMaterialCompiler;

namespace FVoxelUtilities
{
#if WITH_EDITOR
	VOXELCORE_API int32 ZeroDerivative(
		FMaterialCompiler& Compiler,
		int32 Index);
#endif
};