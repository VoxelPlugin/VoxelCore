// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FVoxelObjectPtr::FVoxelObjectPtr(const UObject* Object)
{
	checkUObjectAccess();

	if (!Object)
	{
		return;
	}

	ObjectIndex = GUObjectArray.ObjectToIndex(Object);
	checkVoxelSlow(ObjectIndex >= 0);

	const FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
	checkVoxelSlow(ObjectItem);

	if (ObjectItem->SerialNumber == 0)
	{
		GUObjectArray.AllocateSerialNumber(ObjectIndex);
	}
	checkVoxelSlow(ObjectItem->SerialNumber != 0);

	ObjectSerialNumber = ObjectItem->SerialNumber;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* FVoxelObjectPtr::Resolve() const
{
	checkUObjectAccess();
	return Resolve_Unsafe();
}

UObject* FVoxelObjectPtr::Resolve_Unsafe() const
{
	if (ObjectSerialNumber == 0)
	{
		return nullptr;
	}

	FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
	if (!ObjectItem ||
		ObjectItem->SerialNumber != ObjectSerialNumber)
	{
		return nullptr;
	}

	if (!GUObjectArray.IsValid(ObjectItem, false))
	{
		return nullptr;
	}

	return static_cast<UObject*>(ObjectItem->UE_506_SWITCH(Object, GetObject()));
}

UObject* FVoxelObjectPtr::Resolve_Ensured() const
{
	UObject* Object = Resolve();
	ensureVoxelSlow(Object);
	return Object;
}

bool FVoxelObjectPtr::IsValid_Slow() const
{
	return Resolve() != nullptr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FName FVoxelObjectPtr::GetFName() const
{
	FVoxelGCScopeGuard Guard;

	const UObject* Object = Resolve();
	if (!Object)
	{
		return STATIC_FNAME("<null>");
	}

	return Object->GetFName();
}

FString FVoxelObjectPtr::GetName() const
{
	return GetFName().ToString();
}

FString FVoxelObjectPtr::GetPathName() const
{
	checkVoxelSlow(IsInGameThread());

	const UObject* Object = Resolve();
	if (!Object)
	{
		return TEXT("<null>");
	}

	return Object->GetPathName();
}

FString FVoxelObjectPtr::GetReadableName() const
{
	checkVoxelSlow(IsInGameThread());
	return FVoxelUtilities::GetReadableName(Resolve());
}