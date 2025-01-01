// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Materials/HLSLMaterialTranslator.h"

#if WITH_EDITOR
int32 FVoxelUtilities::ZeroDerivative(
	FMaterialCompiler& Compiler,
	const int32 Index)
{
	if (Index == -1)
	{
		return -1;
	}

	struct FHack : FHLSLMaterialTranslator
	{
		void Fix(const int32 Index) const
		{
			(*CurrentScopeChunks)[Index].DerivativeStatus = EDerivativeStatus::Zero;
		}
	};
	static_cast<FHack&>(Compiler).Fix(Index);

	return Index;
}
#endif