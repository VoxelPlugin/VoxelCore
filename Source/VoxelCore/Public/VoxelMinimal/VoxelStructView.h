// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelSharedPtr.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

class FStructOnScope;
class FVoxelStructView;
struct FVoxelVirtualStruct;
struct FVoxelInstancedStruct;

template<typename Struct>
UScriptStruct* StaticStructFast();

class VOXELCORE_API FConstVoxelStructView
{
public:
	FConstVoxelStructView() = default;

	FORCEINLINE FConstVoxelStructView(
		UScriptStruct* ScriptStruct,
		const void* StructMemory)
		: ScriptStruct(ScriptStruct)
		, StructMemory(StructMemory)
	{
		checkVoxelSlow((ScriptStruct != nullptr) == (StructMemory != nullptr));
	}

	FConstVoxelStructView(const FStructOnScope& StructOnScope);
	FConstVoxelStructView(const FVoxelInstancedStruct& InstancedStruct);

	template<typename T>
	FORCEINLINE static FConstVoxelStructView Make(const T& Value)
	{
		if constexpr (std::derived_from<T, FVoxelVirtualStruct>)
		{
			return FConstVoxelStructView(Value.GetStruct(), &Value);
		}
		else
		{
			return FConstVoxelStructView(StaticStructFast<T>(), &Value);
		}
	}

public:
	FSharedVoidRef MakeSharedCopy() const;
	FVoxelInstancedStruct MakeInstancedStruct() const;

	bool Identical(FConstVoxelStructView Other) const;
	void CopyTo(FVoxelStructView Other) const;

	FORCEINLINE TConstVoxelArrayView<uint8> GetRawView() const
	{
		checkVoxelSlow(IsValid());
		return TConstVoxelArrayView<uint8>(static_cast<const uint8*>(GetStructMemory()), GetScriptStruct()->GetStructureSize());
	}

public:
	FORCEINLINE UScriptStruct* GetScriptStruct() const
	{
#if VOXEL_DEBUG
		CheckSlow();
#endif
		return ScriptStruct;
	}
	FORCEINLINE const void* GetStructMemory() const
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
		return
			IsValid() &&
			GetScriptStruct()->IsChildOf(StaticStructFast<OtherType>());
	}
	template<typename T>
	FORCEINLINE const T& Get() const
	{
		checkVoxelSlow(IsA<T>());
		return *static_cast<const T*>(StructMemory);
	}

protected:
	UScriptStruct* ScriptStruct = nullptr;
	const void* StructMemory = nullptr;

	void CheckSlow() const;
};

class FVoxelStructView : public FConstVoxelStructView
{
public:
	FVoxelStructView() = default;

	FORCEINLINE FVoxelStructView(
		UScriptStruct* ScriptStruct,
		void* StructMemory)
		: FConstVoxelStructView(ScriptStruct, StructMemory)
	{
	}

	FORCEINLINE FVoxelStructView(FStructOnScope& StructOnScope)
		: FConstVoxelStructView(StructOnScope)
	{
	}
	FORCEINLINE FVoxelStructView(FVoxelInstancedStruct& InstancedStruct)
		: FConstVoxelStructView(InstancedStruct)
	{
	}

	template<typename T>
	FORCEINLINE static FVoxelStructView Make(T& Value)
	{
		if constexpr (std::derived_from<T, FVoxelVirtualStruct>)
		{
			return FVoxelStructView(Value.GetStruct(), &Value);
		}
		else
		{
			return FVoxelStructView(StaticStructFast<T>(), &Value);
		}
	}

public:
	FORCEINLINE TVoxelArrayView<uint8> GetRawView() const
	{
		checkVoxelSlow(IsValid());
		return TVoxelArrayView<uint8>(static_cast<uint8*>(GetStructMemory()), GetScriptStruct()->GetStructureSize());
	}

public:
	FORCEINLINE void* GetStructMemory() const
	{
		return ConstCast(StructMemory);
	}

	template<typename T>
	FORCEINLINE T& Get() const
	{
		checkVoxelSlow(IsA<T>());
		return *static_cast<T*>(ConstCast(StructMemory));
	}
};