// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

uint64 FVoxelUtilities::MurmurHashBytes(const TConstVoxelArrayView<uint8> Bytes, const uint32 Seed)
{
	constexpr int32 WordSize = sizeof(uint32);
	const int32 Size = Bytes.Num() / WordSize;
	const uint32* RESTRICT Hash = reinterpret_cast<const uint32*>(Bytes.GetData());

	uint32 H = 1831214719 * (1460481823 + Seed);
	for (int32 Index = 0; Index < Size; Index++)
	{
		uint32 K = Hash[Index];
		K *= 0xcc9e2d51;
		K = (K << 15) | (K >> 17);
		K *= 0x1b873593;
		H ^= K;
		H = (H << 13) | (H >> 19);
		H = H * 5 + 0xe6546b64;
	}

	uint32 Tail = 0;
	for (int32 Index = Size * WordSize; Index < Bytes.Num(); Index++)
	{
		Tail = (Tail << 8) | Bytes[Index];
	}

	return MurmurHash64(H ^ Tail ^ uint32(Size));
}

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

uint64 FVoxelUtilities::HashString(const FStringView& Name)
{
	return CityHash64(
		reinterpret_cast<const char*>(Name.GetData()),
		Name.Len() * sizeof(TCHAR));
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