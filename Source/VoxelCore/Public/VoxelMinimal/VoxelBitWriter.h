// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

class VOXELCORE_API FVoxelBitWriter
{
public:
	FVoxelBitWriter()
	{
		Buffer.Reserve(2048);
	}

	FORCEINLINE void Reset()
	{
		Buffer.Reset();
		PendingBits = 0;
		NumPendingBits = 0;
	}

	FORCEINLINE TConstVoxelArrayView<uint8> GetByteData() const
	{
		checkVoxelSlow(NumPendingBits == 0);
		return Buffer;
	}
	FORCEINLINE TConstVoxelArrayView<uint32> GetWordData() const
	{
		checkVoxelSlow(NumPendingBits == 0);
		checkVoxelSlow(Buffer.Num() % 4 == 0);
		return MakeVoxelArrayView(Buffer).ReinterpretAs<uint32>();
	}

	FORCEINLINE void Append(const uint32 Bits, const uint32 NumBits)
	{
		checkVoxelSlow(NumPendingBits < 8);
		checkVoxelSlow(uint64(Bits) < (1ull << NumBits));

		PendingBits |= uint64(Bits) << NumPendingBits;
		NumPendingBits += NumBits;

		while (NumPendingBits >= 8)
		{
			Buffer.Add(uint8(PendingBits));
			PendingBits >>= 8;
			NumPendingBits -= 8;
		}
	}
	// Will append 0s until the end of the current byte/word depending on Alignment
	FORCEINLINE void Flush(const uint32 Alignment)
	{
		checkVoxelSlow(NumPendingBits < 8);

		if (NumPendingBits > 0)
		{
			Buffer.Add(uint8(PendingBits));
			PendingBits = 0;
			NumPendingBits = 0;
		}
		checkVoxelSlow(PendingBits == 0);

		while (Buffer.Num() % Alignment != 0)
		{
			Buffer.Add(0);
		}
	}

private:
	TVoxelArray<uint8> Buffer;
	uint64 PendingBits = 0;
	int32 NumPendingBits = 0;
};