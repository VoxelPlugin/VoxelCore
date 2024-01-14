// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelObjectWithGuid.h"

void UVoxelObjectWithGuid::PostLoad()
{
	Super::PostLoad();

	if (IsTemplate())
	{
		return;
	}

	if (!PrivateGuid.IsValid())
	{
		PrivateGuid = FGuid::NewGuid();
	}
}

void UVoxelObjectWithGuid::PostInitProperties()
{
	Super::PostInitProperties();

	if (IsTemplate())
	{
		return;
	}

	if (!PrivateGuid.IsValid())
	{
		PrivateGuid = FGuid::NewGuid();
	}
}

void UVoxelObjectWithGuid::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	PrivateGuid = FGuid::NewGuid();
}