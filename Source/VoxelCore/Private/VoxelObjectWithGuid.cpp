// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelObjectWithGuid.h"

FGuid UVoxelObjectWithGuid::GetGuid() const
{
	if (!PrivateGuid.IsValid())
	{
		ConstCast(this)->UpdateGuid();
	}
	return PrivateGuid;
}

void UVoxelObjectWithGuid::PostLoad()
{
	Super::PostLoad();

	if (IsTemplate())
	{
		return;
	}

	UpdateGuid();
}

void UVoxelObjectWithGuid::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	PrivateGuid = FGuid::NewGuid();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelObjectWithGuid::UpdateGuid()
{
	if (PrivateGuid.IsValid())
	{
		return;
	}
	PrivateGuid = FGuid::NewGuid();

	FVoxelUtilities::DelayedCall(MakeWeakObjectPtrLambda(this, [=]
	{
		LOG_VOXEL(Warning, "Marking %s as dirty", *GetPathName());
		(void)MarkPackageDirty();
	}));
}