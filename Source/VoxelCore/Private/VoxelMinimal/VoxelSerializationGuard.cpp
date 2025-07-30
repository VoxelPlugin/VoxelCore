// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FVoxelSerializationGuard::FVoxelSerializationGuard(FArchive& Ar)
	: Ar(Ar)
	, Offset(Ar.Tell())
{
	if (!Ar.IsLoading() &&
		!Ar.IsSaving())
	{
		return;
	}

	if (Offset == -1)
	{
		// eg FPackageHarvester
		return;
	}

	Ar << SerializedSize;

	if (Ar.IsLoading())
	{
		ensure(Offset + SerializedSize <= Ar.TotalSize());
	}
}

FVoxelSerializationGuard::~FVoxelSerializationGuard()
{
	if (!Ar.IsLoading() &&
		!Ar.IsSaving())
	{
		return;
	}

	if (Offset == -1)
	{
		// eg FPackageHarvester
		return;
	}

	if (Ar.IsLoading())
	{
		const int64 ActualSerializedSize = Ar.Tell() - Offset;

		if (!ensure(ActualSerializedSize == SerializedSize))
		{
			Ar.Seek(Offset + SerializedSize);
			ensure(Ar.Tell() == Offset + SerializedSize);
		}
	}
	else
	{
		check(Ar.IsSaving());

		const int64 NewOffset = Ar.Tell();

		Ar.Seek(Offset);
		check(Ar.Tell() == Offset);

		SerializedSize = NewOffset - Offset;
		Ar << SerializedSize;

		Ar.Seek(NewOffset);
		check(Ar.Tell() == NewOffset);
	}
}