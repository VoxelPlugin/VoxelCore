﻿// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "UObject/StructOnScope.h"
#include "VoxelMinimal/VoxelInstancedStruct.h"

template<typename T>
class TVoxelStructView
{
public:
	TVoxelStructView() = default;

	FORCEINLINE TVoxelStructView(UScriptStruct* ScriptStruct, T* StructMemory)
		: ScriptStruct(ScriptStruct)
		, StructMemory(StructMemory)
	{
		checkVoxelSlow((ScriptStruct != nullptr) == (StructMemory != nullptr));
	}
	FORCEINLINE TVoxelStructView(FVoxelInstancedStruct& InstancedStruct)
	{
		if constexpr (std::is_void_v<T>)
		{
			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.GetStructMemory();
		}
		else
		{
			checkVoxelSlow(!InstancedStruct.IsValid() || InstancedStruct.IsA<T>());
			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.GetPtr<T>();
		}
	}
	FORCEINLINE TVoxelStructView(const FVoxelInstancedStruct& InstancedStruct)
	{
		if constexpr (std::is_void_v<T>)
		{
			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.GetStructMemory();
		}
		else
		{
			checkVoxelSlow(!InstancedStruct.IsValid() || InstancedStruct.IsA<T>());
			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.GetPtr<T>();
		}
	}
	FORCEINLINE TVoxelStructView(const FStructOnScope& StructOnScope)
	{
		ScriptStruct = ConstCast(CastChecked<UScriptStruct>(StructOnScope.GetStruct(), ECastCheckedType::NullAllowed));
		StructMemory = reinterpret_cast<T*>(ConstCast(StructOnScope.GetStructMemory()));

		if constexpr (!std::is_void_v<T>)
		{
			checkVoxelSlow(IsA<T>());
		}
	}

	template<typename OtherType, typename = std::enable_if_t<
		!std::is_same_v<T, OtherType>
		&&
		(
			!std::is_const_v<OtherType> ||
			std::is_const_v<T>
		)
		&&
		(
			std::is_void_v<T> ||
			std::is_void_v<OtherType> ||
			std::derived_from<OtherType, T>
		)>>
	FORCEINLINE TVoxelStructView(const TVoxelStructView<OtherType>& Other)
	{
		ScriptStruct = Other.GetStruct();
		StructMemory = Other.GetMemory();
	}

	template<typename ChildType, typename = std::enable_if_t<std::is_const_v<T> && (std::is_void_v<T> || std::derived_from<ChildType, T>)>>
	FORCEINLINE TVoxelStructView(TVoxelInstancedStruct<ChildType>& InstancedStruct)
	{
		checkStatic(!std::is_const_v<ChildType>);

		if constexpr (std::is_void_v<T>)
		{
			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.GetPtr();
		}
		else
		{
			checkVoxelSlow(!InstancedStruct.IsValid() || InstancedStruct.template IsA<T>());

			ScriptStruct = InstancedStruct.GetScriptStruct();
			StructMemory = InstancedStruct.template GetPtr<T>();
		}
	}

	template<typename ChildType, typename = std::enable_if_t<std::is_const_v<T> && (std::is_void_v<T> || std::derived_from<ChildType, T>)>>
	FORCEINLINE TVoxelStructView(const TVoxelInstancedStruct<ChildType>& InstancedStruct)
		: TVoxelStructView(ConstCast(InstancedStruct))
	{
	}

public:
	auto MakeInstancedStruct() const
	{
		FVoxelInstancedStruct Result;
		Result.InitializeAs(GetStruct(), GetMemory());

		using InstancedStruct = std::conditional_t<std::is_void_v<T>, FVoxelInstancedStruct, TVoxelInstancedStruct<T>>;
		return InstancedStruct(MoveTemp(Result));
	}
	auto MakeSharedCopy() const
	{
		if constexpr (std::is_void_v<T>)
		{
			return MakeSharedStruct(GetStruct(), GetMemory());
		}
		else
		{
			return MakeSharedStruct<T>(GetStruct(), GetMemory());
		}
	}

	FORCEINLINE TVoxelArrayView<std::conditional_t<std::is_const_v<T>, const uint8, uint8>> GetRawView() const
	{
		checkVoxelSlow(IsValid());
		using ByteType = std::conditional_t<std::is_const_v<T>, const uint8, uint8>;
		return TVoxelArrayView<ByteType>(reinterpret_cast<ByteType*>(GetMemory()), GetStruct()->GetStructureSize());
	}

	FORCEINLINE bool Identical(const TVoxelStructView<const T>& Other) const
	{
		checkVoxelSlow(IsValid());
		checkVoxelSlow(Other.IsValid());
		checkVoxelSlow(GetStruct() == Other.GetStruct());
		return GetStruct()->CompareScriptStruct(Other.GetMemory(), GetMemory(), PPF_None);
	}
	FORCEINLINE void CopyTo(const TVoxelStructView<std::remove_const_t<T>>& Other) const
	{
		checkVoxelSlow(IsValid());
		checkVoxelSlow(Other.IsValid());
		checkVoxelSlow(GetStruct() == Other.GetStruct());

		if (GetStruct()->StructFlags & STRUCT_CopyNative)
		{
			checkVoxelSlow(!(GetStruct()->StructFlags & STRUCT_IsPlainOldData));

			if (!ensureVoxelSlow(GetStruct()->GetCppStructOps()->Copy(Other.GetMemory(), GetMemory(), 1)))
			{
				GetStruct()->CopyScriptStruct(Other.GetMemory(), GetMemory());
			}
		}
		else if (GetStruct()->StructFlags & STRUCT_IsPlainOldData)
		{
			FMemory::Memcpy(Other.GetMemory(), GetMemory(), GetStruct()->GetStructureSize());
		}
		else
		{
			GetStruct()->CopyScriptStruct(Other.GetMemory(), GetMemory());
		}
	}

public:
	FORCEINLINE UScriptStruct* GetStruct() const
	{
#if VOXEL_DEBUG
		if constexpr (std::is_void_v<T> || std::derived_from<T, FVoxelVirtualStruct>)
		{
			if (ScriptStruct &&
				ScriptStruct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
			{
				checkVoxelSlow(ScriptStruct == static_cast<const FVoxelVirtualStruct*>(StructMemory)->Internal_GetStruct());
			}
		}
#endif
		return ScriptStruct;
	}
	FORCEINLINE T* GetMemory() const
	{
		return StructMemory;
	}

	FORCEINLINE bool IsValid() const
	{
		checkVoxelSlow((ScriptStruct != nullptr) == (StructMemory != nullptr));
		return ScriptStruct != nullptr;
	}

	template<typename OtherType>
	FORCEINLINE bool IsA() const
	{
		checkVoxelSlow(
			!IsValid() ||
			!ScriptStruct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()) ||
			reinterpret_cast<const FVoxelVirtualStruct*>(StructMemory)->GetStruct() == ScriptStruct);

		return
			IsValid() &&
			ScriptStruct->IsChildOf(StaticStructFast<OtherType>());
	}
	template<typename OtherType = T>
	FORCEINLINE auto& Get() const
	{
		checkVoxelSlow(IsA<OtherType>());
		return *static_cast<std::conditional_t<std::is_const_v<T>, const OtherType, OtherType>*>(StructMemory);
	}

	template<typename OtherType = T>
	FORCEINLINE auto operator*() const -> decltype(auto)
	{
		return Get<OtherType>();
	}
	template<typename OtherType = T>
	FORCEINLINE auto operator->() const -> decltype(auto)
	{
		return &Get<OtherType>();
	}

private:
	UScriptStruct* ScriptStruct = nullptr;
	T* StructMemory = nullptr;

	template<typename>
	friend class TVoxelStructView;
};

template<typename T>
FORCEINLINE TVoxelStructView<T> MakeVoxelStructView(T& Value)
{
	UScriptStruct* ScriptStruct;
	if constexpr (std::derived_from<T, FVoxelVirtualStruct>)
	{
		ScriptStruct = Value.GetStruct();
	}
	else
	{
		ScriptStruct = StaticStructFast<T>();
	}
	return TVoxelStructView<T>(ScriptStruct, &Value);
}

template<typename T>
using TConstVoxelStructView = TVoxelStructView<const T>;

using FVoxelStructView = TVoxelStructView<void>;
using FConstVoxelStructView = TConstVoxelStructView<void>;

VOXELCORE_API void ForeachObjectReference(
	FVoxelStructView Struct,
	TFunctionRef<void(UObject*& ObjectRef)> Lambda,
	EPropertyFlags SkipFlags = CPF_Transient);