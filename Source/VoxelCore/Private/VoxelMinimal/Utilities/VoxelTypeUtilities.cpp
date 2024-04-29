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

	checkStatic(!FVoxelUtilities::IsForceInitializeable_V<FTestNoForceInit>);
	checkStatic(!FVoxelUtilities::IsForceInitializeable_V<FTestNoForceInit2>);
	checkStatic(FVoxelUtilities::IsForceInitializeable_V<FVector>);
}