// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	// Don't allow converting EForceInit to int32
	template<typename T>
	struct TConvertibleOnlyTo
	{
		template<typename S>
		requires std::is_same_v<T, S>
		operator S() const
		{
			return S{};
		}
	};

	template<typename T>
	static constexpr bool IsForceInitializable_V = std::is_constructible_v<T, TConvertibleOnlyTo<EForceInit>>;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	static constexpr bool CanMakeSafe =
		!TIsTSharedRef_V<T> &&
		!std::derived_from<T, UObject> &&
		(std::is_constructible_v<T> || IsForceInitializable_V<T>);

	template<typename T>
	requires
	(
		CanMakeSafe<T> &&
		!IsForceInitializable_V<T>
	)
	T MakeSafe()
	{
		return T{};
	}

	template<typename T>
	requires
	(
		CanMakeSafe<T> &&
		IsForceInitializable_V<T>
	)
	T MakeSafe()
	{
		return T(ForceInit);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	requires std::is_trivially_destructible_v<T>
	T MakeZeroed()
	{
		T Value;
		FMemory::Memzero(Value);
		return Value;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename From, typename To>
	struct TCanCastMemoryImpl : std::false_type {};

	template<typename From, typename To>
	static constexpr bool CanCastMemory = TCanCastMemoryImpl<From, To>::value;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct FVoxelUtilities::TCanCastMemoryImpl<T, T> : std::true_type {};

template<typename From, typename To>
struct FVoxelUtilities::TCanCastMemoryImpl<TSharedRef<From>, TSharedPtr<To>> : TCanCastMemoryImpl<TSharedPtr<From>, TSharedPtr<To>> {};

template<typename From, typename To>
requires
(
	!std::is_const_v<From>
)
struct FVoxelUtilities::TCanCastMemoryImpl<From, const To> : TCanCastMemoryImpl<From, To> {};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename From, typename To>
requires
(
	!std::is_const_v<From>
)
struct FVoxelUtilities::TCanCastMemoryImpl<From*, const To*> : TCanCastMemoryImpl<From*, To*> {};

template<typename From, typename To>
requires
(
	!std::is_same_v<From, To> &&
	std::derived_from<From, To> &&
	std::is_const_v<From> == std::is_const_v<To>
)
struct FVoxelUtilities::TCanCastMemoryImpl<From*, To*> : std::true_type {};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename From, typename To>
requires
(
	!std::is_const_v<From>
)
struct FVoxelUtilities::TCanCastMemoryImpl<TSharedPtr<From>, TSharedPtr<const To>> : TCanCastMemoryImpl<TSharedPtr<From>, TSharedPtr<To>> {};

template<typename From, typename To>
requires
(
	!std::is_same_v<From, To> &&
	std::derived_from<From, To> &&
	std::is_const_v<From> == std::is_const_v<To>
)
struct FVoxelUtilities::TCanCastMemoryImpl<TSharedPtr<From>, TSharedPtr<To>> : std::true_type {};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename From, typename To>
requires
(
	!std::is_const_v<From>
)
struct FVoxelUtilities::TCanCastMemoryImpl<TSharedRef<From>, TSharedRef<const To>> : TCanCastMemoryImpl<TSharedRef<From>, TSharedRef<To>> {};

template<typename From, typename To>
requires
(
	!std::is_same_v<From, To> &&
	std::derived_from<From, To> &&
	std::is_const_v<From> == std::is_const_v<To>
)
struct FVoxelUtilities::TCanCastMemoryImpl<TSharedRef<From>, TSharedRef<To>> : std::true_type {};