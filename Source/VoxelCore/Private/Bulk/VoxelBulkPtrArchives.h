// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelBulkPtrWriter : public FVoxelWriterArchive
{
public:
	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrWriter";
	}
	//~ End FArchive Interface
};

class FVoxelBulkPtrReader : public FVoxelReaderArchive
{
public:
	using FVoxelReaderArchive::FVoxelReaderArchive;

	//~ Begin FArchive Interface
	virtual FString GetArchiveName() const override
	{
		return "FVoxelBulkPtrReader";
	}
	//~ End FArchive Interface
};