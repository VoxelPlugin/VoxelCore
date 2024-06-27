// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FGuid FVoxelUtilities::CombineGuids(const TConstVoxelArrayView<FGuid> Guids)
{
	const uint64 AB = MurmurHashBytes(MakeByteVoxelArrayView(Guids), 0);
	const uint64 CD = MurmurHashBytes(MakeByteVoxelArrayView(Guids), 1);

	return FGuid(
		ReinterpretCastRef<uint32[2]>(AB)[0],
		ReinterpretCastRef<uint32[2]>(AB)[1],
		ReinterpretCastRef<uint32[2]>(CD)[0],
		ReinterpretCastRef<uint32[2]>(CD)[1]);
}

FSHAHash FVoxelUtilities::ShaHash(const TConstVoxelArrayView64<uint8> Data)
{
	VOXEL_FUNCTION_COUNTER();

	if (Data.Num() == 0)
	{
		return {};
	}

	return FSHA1::HashBuffer(Data.GetData(), Data.Num());
}