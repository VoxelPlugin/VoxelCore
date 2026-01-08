// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelBulkPtrWriter : public FVoxelWriter
{
public:
	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrWriter";
	}
	//~ End FArchive Interface
};

class FVoxelBulkPtrReader : public FVoxelReader
{
public:
	using FVoxelReader::FVoxelReader;

	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrReader";
	}
	//~ End FArchive Interface
};

class FVoxelBulkPtrShallowArchive : public FArchiveProxy
{
public:
	using FArchiveProxy::FArchiveProxy;

	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrShallowArchive";
	}
	//~ End FArchive Interface
};