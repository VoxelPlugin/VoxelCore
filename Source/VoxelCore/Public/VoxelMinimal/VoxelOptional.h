// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename Type>
struct TVoxelOptional
{
public:
	TVoxelOptional() = default;

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptional(const OtherType& Value)
	{
		new(Storage.GetTypedPtr()) Type(Value);
		bIsSet = true;
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptional(OtherType&& Value)
	{
		new(Storage.GetTypedPtr()) Type(MoveTemp(Value));
		bIsSet = true;
	}

	FORCEINLINE ~TVoxelOptional()
	{
		Reset();
	}

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptional(const TVoxelOptional<OtherType>& Other)
	{
		if (Other.bIsSet)
		{
			new(Storage.GetTypedPtr()) Type(Other.GetValue());
			bIsSet = true;
		}
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptional(TVoxelOptional<OtherType>&& Other)
	{
		if (Other.bIsSet)
		{
			new(Storage.GetTypedPtr()) Type(MoveTemp(Other.GetValue()));
			bIsSet = true;
			Other.Reset();
		}
	}

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptional& operator=(const TVoxelOptional<OtherType>& Other)
	{
		Reset();
		new(this) TVoxelOptional(Other);
		return *this;
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptional& operator=(TVoxelOptional<OtherType>&& Other)
	{
		Reset();
		new(this) TVoxelOptional(MoveTemp(Other));
		return *this;
	}

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptional& operator=(const OtherType& Value)
	{
		this->Emplace(Value);
		return *this;
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptional& operator=(OtherType&& Value)
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

	template<typename... ArgsType>
	requires std::is_constructible_v<Type, ArgsType&&...>
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