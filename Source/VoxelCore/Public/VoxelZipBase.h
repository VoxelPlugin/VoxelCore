// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "miniz.h"

class VOXELCORE_API FVoxelZipBase
{
public:
	FVoxelZipBase();
	~FVoxelZipBase();

	FORCEINLINE bool HasError() const
	{
		return bHasError.Get();
	}

protected:
	struct FOodleHeader
	{
		TVoxelStaticArray<char, 8> Tag = { 'O', 'O', 'D', 'L', 'E', '_', 'V', 'O' };
		int64 UncompressedSize = 0;
		int64 CompressedSize = 0;
	};

	mutable mz_zip_archive Archive;

	void CheckError() const;
	void RaiseError() const;

private:
	mutable TVoxelAtomic<bool> bHasError;
};