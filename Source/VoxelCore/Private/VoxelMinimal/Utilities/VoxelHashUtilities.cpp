// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FSHAHash FVoxelUtilities::ShaHash(const TConstVoxelArrayView64<uint8> Data)
{
	VOXEL_FUNCTION_COUNTER();

	if (Data.Num() == 0)
	{
		return {};
	}

	return FSHA1::HashBuffer(Data.GetData(), Data.Num());
}