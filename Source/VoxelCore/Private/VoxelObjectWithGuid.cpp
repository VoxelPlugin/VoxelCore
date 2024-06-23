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

#if WITH_EDITOR
	static TVoxelMap<FGuid, TWeakObjectPtr<UVoxelObjectWithGuid>> GuidToObject;

	if (GuidToObject.Contains(PrivateGuid))
	{
		if (UVoxelObjectWithGuid* Object = GuidToObject.FindChecked(PrivateGuid).Get())
		{
			if (Object != this &&
				Object->PrivateGuid == PrivateGuid)
			{
				VOXEL_MESSAGE(Error, "Objects with conflicting GUIDs: {0} and {1}", this, Object);
			}
		}
	}

	GuidToObject.FindOrAdd(PrivateGuid) = this;
#endif
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

	FVoxelUtilities::DelayedCall(MakeWeakObjectPtrLambda(this, [this]
	{
		LOG_VOXEL(Warning, "Marking %s as dirty", *GetPathName());
		(void)MarkPackageDirty();
	}));
}