// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	template<typename T>
	class TConvertibleOnlyTo
	{
	public:
		template<typename S, typename = std::enable_if_t<std::is_same_v<T, S>>>
		operator S() const
		{
			return S{};
		}
	};

	template<typename T>
	static constexpr bool IsForceInitializable_V = std::is_constructible_v<T, TConvertibleOnlyTo<EForceInit>>;

	template<typename T>
	static constexpr bool CanMakeSafe =
		!TIsTSharedRef_V<T> &&
		!TIsDerivedFrom<T, UObject>::Value;

	template<typename T, typename = std::enable_if_t<CanMakeSafe<T>>>
	std::enable_if_t<!IsForceInitializable_V<T>, T> MakeSafe()
	{
		return T{};
	}
	template<typename T, typename = std::enable_if_t<CanMakeSafe<T>>>
	std::enable_if_t<IsForceInitializable_V<T>, T> MakeSafe()
	{
		return T(ForceInit);
	}

	template<typename T>
	T MakeZeroed()
	{
		T Value;
		FMemory::Memzero(Value);
		return Value;
	}
}