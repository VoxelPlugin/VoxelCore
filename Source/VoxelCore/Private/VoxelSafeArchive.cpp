// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSafeArchive.h"

void FVoxelSafeArchive::SerializeIntPacked64(uint64& Value)
{
	if (HasLoadingError())
	{
		Value = 0;
		return;
	}

	FArchiveProxy::SerializeIntPacked64(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FName& Value)
{
	if (HasLoadingError())
	{
		Value = {};
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FText& Value)
{
	if (HasLoadingError())
	{
		Value = {};
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(UObject*& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FObjectPtr& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FLazyObjectPtr& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FSoftObjectPath& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FSoftObjectPtr& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FWeakObjectPtr& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

FArchive& FVoxelSafeArchive::operator<<(FField*& Value)
{
	if (HasLoadingError())
	{
		Value = nullptr;
		return *this;
	}

	return FArchiveProxy::operator<<(Value);
}

void FVoxelSafeArchive::Serialize(void* V, const int64 Length)
{
	if (HasLoadingError())
	{
		FMemory::Memzero(V, Length);
		return;
	}

	FArchiveProxy::Serialize(V, Length);
}

void FVoxelSafeArchive::SerializeBits(void* V, const int64 LengthBits)
{
	if (HasLoadingError())
	{
		FMemory::Memzero(V, FMath::DivideAndRoundUp<int64>(LengthBits, 8));
		return;
	}

	FArchiveProxy::SerializeBits(V, LengthBits);
}

void FVoxelSafeArchive::SerializeInt(uint32& Value, const uint32 Max)
{
	if (HasLoadingError())
	{
		Value = 0;
		return;
	}

	FArchiveProxy::SerializeInt(Value, Max);
}

void FVoxelSafeArchive::SerializeIntPacked(uint32& Value)
{
	if (HasLoadingError())
	{
		Value = 0;
		return;
	}

	FArchiveProxy::SerializeIntPacked(Value);
}