// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename Type>
struct TVoxelOptional
{
public:
	TVoxelOptional() = default;

	FORCEINLINE TVoxelOptional(const Type& Value)
	{
		new(Storage.GetTypedPtr()) Type(Value);
		bIsSet = true;
	}
	FORCEINLINE TVoxelOptional(Type&& Value)
	{
		new(Storage.GetTypedPtr()) Type(MoveTemp(Value));
		bIsSet = true;
	}

	FORCEINLINE ~TVoxelOptional()
	{
		Reset();
	}

	FORCEINLINE TVoxelOptional(const TVoxelOptional& Other)
	{
		if (Other.bIsSet)
		{
			new(Storage.GetTypedPtr()) Type(Other.GetValue());
			bIsSet = true;
		}
	}
	FORCEINLINE TVoxelOptional(TVoxelOptional&& Other)
	{
		if (Other.bIsSet)
		{
			FMemory::Memcpy(Storage, Other.Storage);
			bIsSet = true;
			Other.bIsSet = false;
		}
	}

	FORCEINLINE TVoxelOptional& operator=(const TVoxelOptional& Other)
	{
		Reset();
		new(this) TVoxelOptional(Other);
		return *this;
	}
	FORCEINLINE TVoxelOptional& operator=(TVoxelOptional&& Other)
	{
		Reset();
		new(this) TVoxelOptional(MoveTemp(Other));
		return *this;
	}

	FORCEINLINE TVoxelOptional& operator=(const Type& Value)
	{
		this->Emplace(Value);
		return *this;
	}
	FORCEINLINE TVoxelOptional& operator=(Type&& Value)
	{
		this->Emplace(MoveTemp(Value));
		return *this;
	}

	FORCEINLINE void Reset()
	{
		if (bIsSet)
		{
			GetValue().~Type();
			bIsSet = false;
		}
	}

	template <typename... ArgsType>
	FORCEINLINE Type& Emplace(ArgsType&&... Args)
	{
		if (bIsSet)
		{
			GetValue().~Type();
		}

		Type* Result = new(Storage.GetTypedPtr()) Type(Forward<ArgsType>(Args)...);
		bIsSet = true;
		return *Result;
	}

	FORCEINLINE bool IsSet() const
	{
		return bIsSet;
	}
	FORCEINLINE explicit operator bool() const
	{
		return bIsSet;
	}

	FORCEINLINE Type& GetValue()
	{
		checkVoxelSlow(bIsSet);
		return *Storage.GetTypedPtr();
	}
	FORCEINLINE const Type& GetValue() const
	{
		checkVoxelSlow(bIsSet);
		return *Storage.GetTypedPtr();
	}

	FORCEINLINE Type* operator->()
	{
		return &GetValue();
	}
	FORCEINLINE const Type* operator->() const
	{
		return &GetValue();
	}

	FORCEINLINE Type& operator*()
	{
		return GetValue();
	}
	FORCEINLINE const Type& operator*() const
	{
		return GetValue();
	}

	FORCEINLINE bool operator==(const TVoxelOptional& Other) const
	{
		if (bIsSet != Other.bIsSet)
		{
			return false;
		}

		if (!bIsSet)
		{
			return true;
		}

		return GetValue() == Other.GetValue();
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelOptional& Optional)
	{
		if (!Optional.IsSet())
		{
			return 0;
		}
		return GetTypeHash(Optional.GetValue());
	}

private:
	TTypeCompatibleBytes<Type> Storage;
	bool bIsSet = false;
};