// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class VOXELCORE_API FVoxelHash
{
public:
	FVoxelHash()
	{
		AB = CD = EF = 0;
	}

	bool operator==(const FVoxelHash& Other) const
	{
		return AB == Other.AB && CD == Other.CD && EF == Other.EF;
	}
	bool operator!=(const FVoxelHash& Other) const
	{
		return AB != Other.AB || CD != Other.CD || EF != Other.EF;
	}

private:
	union
	{
		struct
		{
			uint64 AB;
			uint64 CD;
			uint64 EF;
		};
		uint8 Raw[20];
	};

	friend class FVoxelHashBuilder;
};

class VOXELCORE_API FVoxelHashBuilder
{
public:
	FVoxelHashBuilder() = default;

	template<typename T>
	FVoxelHashBuilder& operator<<(const T& Value)
	{
		checkStatic(TIsTriviallyDestructible<T>::Value);
		Sha.Update(reinterpret_cast<const uint8*>(&Value), sizeof(Value));
		return *this;
	}

	template<typename T>
	FVoxelHashBuilder& operator<<(const TArray<T>& Array)
	{
		if (Array.Num() == 0)
		{
			return *this;
		}

		VOXEL_FUNCTION_COUNTER();

		checkStatic(TIsTriviallyDestructible<T>::Value);
		Sha.Update(reinterpret_cast<const uint8*>(Array.GetData()), Array.Num() * Array.GetTypeSize());
		return *this;
	}

	FVoxelHash MakeHash()
	{
		VOXEL_FUNCTION_COUNTER();

		FVoxelHash Hash;
		Sha.Final();
		Sha.GetHash(Hash.Raw);
		Sha.Reset();
		return Hash;
	}

	operator FVoxelHash()
	{
		return MakeHash();
	}

private:
	FSHA1 Sha;
};