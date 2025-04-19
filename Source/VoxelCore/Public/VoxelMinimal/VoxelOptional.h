// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelTypeCompatibleBytes.h"

template<typename Type>
struct TVoxelOptional;

template<typename Type>
FORCEINLINE uint32 GetTypeHash(const TVoxelOptional<Type>& Optional)
{
	if (!Optional.IsSet())
	{
		return 0;
	}
	return GetTypeHash(Optional.GetValue());
}

template<typename TypeA, typename TypeB>
requires std::equality_comparable_with<TypeA, TypeB>
FORCEINLINE bool operator==(
	const TVoxelOptional<TypeA>& A,
	const TVoxelOptional<TypeB>& B)
{
	if (A.IsSet() != B.IsSet())
	{
		return false;
	}

	if (!A.IsSet())
	{
		return true;
	}

	return A.GetValue() == B.GetValue();
}

template<typename TypeA, typename TypeB>
requires std::equality_comparable_with<TypeA, TypeB>
FORCEINLINE bool operator==(
	const TVoxelOptional<TypeA>& A,
	const TypeB& B)
{
	if (!A.IsSet())
	{
		return false;
	}

	return A.GetValue() == B;
}

template<typename TypeA, typename TypeB>
requires std::equality_comparable_with<TypeA, TypeB>
FORCEINLINE bool operator==(
	const TypeA& A,
	const TVoxelOptional<TypeB>& B)
{
	if (!B.IsSet())
	{
		return false;
	}

	return A == B.GetValue();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Type, typename ThisType>
struct TVoxelOptionalBase
{
public:
#if INTELLISENSE_PARSER // Invalid "Uninitialized variable" when using = default
	TVoxelOptionalBase() {}
#else
	TVoxelOptionalBase() = default;
#endif

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptionalBase(const OtherType& Value)
	{
		new(Storage) Type(Value);
		bIsSet = true;
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptionalBase(OtherType&& Value)
	{
		new(Storage) Type(Forward<OtherType>(Value));
		bIsSet = true;
	}

	FORCEINLINE ~TVoxelOptionalBase()
	{
		Reset();
	}

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE TVoxelOptionalBase(const TVoxelOptional<OtherType>& Other)
	{
		if (Other.bIsSet)
		{
			new(Storage) Type(Other.GetValue());
			bIsSet = true;
		}
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE TVoxelOptionalBase(TVoxelOptional<OtherType>&& Other)
	{
		if (Other.bIsSet)
		{
			new(Storage) Type(MoveTemp(Other.GetValue()));
			bIsSet = true;
			Other.Reset();
		}
	}

public:
	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE ThisType& operator=(const TVoxelOptional<OtherType>& Other)
	{
		Reset();
		new(this) ThisType(Other);
		return static_cast<ThisType&>(*this);
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE ThisType& operator=(TVoxelOptional<OtherType>&& Other)
	{
		Reset();
		new(this) ThisType(MoveTemp(Other));
		return static_cast<ThisType&>(*this);
	}

	template<typename OtherType>
	requires std::is_convertible_v<const OtherType&, Type>
	FORCEINLINE ThisType& operator=(const OtherType& Value)
	{
		this->Emplace(Value);
		return static_cast<ThisType&>(*this);
	}
	template<typename OtherType>
	requires std::is_convertible_v<OtherType&&, Type>
	FORCEINLINE ThisType& operator=(OtherType&& Value)
	{
		this->Emplace(Forward<OtherType>(Value));
		return static_cast<ThisType&>(*this);
	}

public:
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

		Type* Result = new(Storage) Type(Forward<ArgsType>(Args)...);
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
		return Storage.GetValue();
	}
	FORCEINLINE const Type& GetValue() const
	{
		checkVoxelSlow(bIsSet);
		return Storage.GetValue();
	}

public:
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

private:
	TVoxelTypeCompatibleBytes<Type> Storage;
	bool bIsSet = false;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Type>
requires
(
	!(
		std::is_move_constructible_v<Type> &&
		std::is_move_assignable_v<Type>
	)
	&&
	!(
		std::is_copy_constructible_v<Type> &&
		std::is_copy_assignable_v<Type>
	)
)
struct TVoxelOptional<Type> : TVoxelOptionalBase<Type, TVoxelOptional<Type>>
{
public:
	using Super = TVoxelOptionalBase<Type, TVoxelOptional<Type>>;
	using Super::Super;
	using Super::operator=;
};

template<typename Type>
requires
(
	(
		std::is_move_constructible_v<Type> &&
		std::is_move_assignable_v<Type>
	)
	&&
	!(
		std::is_copy_constructible_v<Type> &&
		std::is_copy_assignable_v<Type>
	)
)
struct TVoxelOptional<Type> : TVoxelOptionalBase<Type, TVoxelOptional<Type>>
{
public:
	using Super = TVoxelOptionalBase<Type, TVoxelOptional<Type>>;
	using Super::Super;
	using Super::operator=;

	FORCEINLINE TVoxelOptional(TVoxelOptional&& Other)
		: Super(MoveTemp(Other))
	{
	}
	FORCEINLINE TVoxelOptional& operator=(TVoxelOptional&& Other)
	{
		Super::operator=(MoveTemp(Other));
		return *this;
	}
};

template<typename Type>
requires
(
	!(
		std::is_move_constructible_v<Type> &&
		std::is_move_assignable_v<Type>
	)
	&&
	(
		std::is_copy_constructible_v<Type> &&
		std::is_copy_assignable_v<Type>
	)
)
struct TVoxelOptional<Type> : TVoxelOptionalBase<Type, TVoxelOptional<Type>>
{
public:
	using Super = TVoxelOptionalBase<Type, TVoxelOptional<Type>>;
	using Super::Super;
	using Super::operator=;

	FORCEINLINE TVoxelOptional(const TVoxelOptional& Other)
		: Super(Other)
	{
	}
	FORCEINLINE TVoxelOptional& operator=(const TVoxelOptional& Other)
	{
		Super::operator=(Other);
		return *this;
	}
};

template<typename Type>
requires
(
	(
		std::is_move_constructible_v<Type> &&
		std::is_move_assignable_v<Type>
	)
	&&
	(
		std::is_copy_constructible_v<Type> &&
		std::is_copy_assignable_v<Type>
	)
)
struct TVoxelOptional<Type> : TVoxelOptionalBase<Type, TVoxelOptional<Type>>
{
public:
	using Super = TVoxelOptionalBase<Type, TVoxelOptional<Type>>;
	using Super::Super;
	using Super::operator=;

	FORCEINLINE TVoxelOptional(const TVoxelOptional& Other)
		: Super(Other)
	{
	}
	FORCEINLINE TVoxelOptional(TVoxelOptional&& Other)
		: Super(MoveTemp(Other))
	{
	}

	FORCEINLINE TVoxelOptional& operator=(const TVoxelOptional& Other)
	{
		Super::operator=(Other);
		return *this;
	}
	FORCEINLINE TVoxelOptional& operator=(TVoxelOptional&& Other)
	{
		Super::operator=(MoveTemp(Other));
		return *this;
	}
};