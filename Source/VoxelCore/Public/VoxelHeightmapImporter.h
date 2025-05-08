// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelHeightmapImporter
{
public:
	const FString Path;
	FString Error;
	FIntPoint Size{ ForceInit };
	int32 BitDepth = 0;
	TArray64<uint8> Data;

	explicit FVoxelHeightmapImporter(const FString& Path)
		: Path(Path)
	{
	}
	virtual ~FVoxelHeightmapImporter() = default;

	virtual bool Import() = 0;

	static TSharedPtr<FVoxelHeightmapImporter> MakeImporter(const FString& Path);
	static bool Import(const FString& Path, FString& OutError, FIntPoint& OutSize, int32& OutBitDepth, TArray64<uint8>& OutData);
};

class VOXELCORE_API FVoxelHeightmapImporter_PNG : public FVoxelHeightmapImporter
{
public:
	using FVoxelHeightmapImporter::FVoxelHeightmapImporter;

	virtual bool Import() override;
};

class VOXELCORE_API FVoxelHeightmapImporter_Raw : public FVoxelHeightmapImporter
{
public:
	using FVoxelHeightmapImporter::FVoxelHeightmapImporter;

	virtual bool Import() override;
};