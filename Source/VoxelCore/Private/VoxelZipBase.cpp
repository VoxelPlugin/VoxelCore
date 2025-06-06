// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelZipBase.h"

FVoxelZipBase::FVoxelZipBase()
{
	mz_zip_zero_struct(&Archive);

	Archive.m_pAlloc = [](void* opaque, const size_t items, const size_t size)
	{
		VOXEL_SCOPE_COUNTER("FMemory::Malloc");
		return FMemory::Malloc(items * size);
	};
	Archive.m_pFree = [](void* opaque, void* address)
	{
		VOXEL_SCOPE_COUNTER("FMemory::Free");
		return FMemory::Free(address);
	};
	Archive.m_pRealloc = [](void* opaque, void* address, const size_t items, const size_t size)
	{
		VOXEL_SCOPE_COUNTER("FMemory::Realloc");
		return FMemory::Realloc(address, items * size);
	};
}

FVoxelZipBase::~FVoxelZipBase()
{
	ensure(mz_zip_end(&Archive));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelZipBase::CheckError() const
{
	const voxel::mz_zip_error ErrorCode = mz_zip_get_last_error(&Archive);
	if (ErrorCode == voxel::MZ_ZIP_NO_ERROR)
	{
		return;
	}

	bHasError.Set(true);

	const FString ErrorString = mz_zip_get_error_string(ErrorCode);
	LOG_VOXEL(Log, "Zip error: %s", *ErrorString);
}

void FVoxelZipBase::RaiseError() const
{
	bHasError.Set(true);
}