// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

namespace FVoxelForceInitTest
{
	struct FTestNoForceInit
	{
		int32 Value = -1;
	};

	struct FTestNoForceInit2
	{
		FTestNoForceInit2(int32 Value) {}
	};

	checkStatic(!FVoxelUtilities::IsForceInitializable_V<FTestNoForceInit>);
	checkStatic(!FVoxelUtilities::IsForceInitializable_V<FTestNoForceInit2>);
	checkStatic(FVoxelUtilities::IsForceInitializable_V<FVector>);
}