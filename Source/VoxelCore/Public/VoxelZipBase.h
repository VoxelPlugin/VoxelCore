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
	mutable voxel::mz_zip_archive Archive;

	void CheckError() const;
	void RaiseError() const;

private:
	mutable TVoxelAtomic<bool> bHasError;
};