// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"

class VOXELCORE_API alignas(8) FVoxelObjectPtr
{
public:
	FVoxelObjectPtr() = default;
	FVoxelObjectPtr(const UObject* Object);
	FORCEINLINE FVoxelObjectPtr(const UObject& Object)
		: FVoxelObjectPtr(&Object)
	{
	}
	FORCEINLINE FVoxelObjectPtr(const FObjectPtr Object)
		: FVoxelObjectPtr(Object.Get())
	{
	}
	FORCEINLINE FVoxelObjectPtr(const FWeakObjectPtr Object)
		: FVoxelObjectPtr(ReinterpretCastRef_Unaligned<FVoxelObjectPtr>(Object))
	{
	}
	FORCEINLINE FVoxelObjectPtr(decltype(nullptr))
	{
	}

public:
	UObject* Resolve() const;
	UObject* Resolve_Ensured() const;
	bool IsValid_Slow() const;

	FName GetFName() const;
	FString GetName() const;
	FString GetPathName() const;
	FString GetReadableName() const;

public:
	FORCEINLINE bool IsExplicitlyNull() const
	{
		checkVoxelSlow((ObjectIndex == 0) == (ObjectSerialNumber == 0));
		return ObjectSerialNumber == 0;
	}

	FORCEINLINE bool operator==(const FVoxelObjectPtr& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef<uint64>(Other);
	}
	FORCEINLINE bool operator==(const UObject* Other) const
	{
		return *this == FVoxelObjectPtr(Other);
	}
	FORCEINLINE bool operator==(const FWeakObjectPtr& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef_Unaligned<uint64>(Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelObjectPtr& ObjectPtr)
	{
		return FVoxelUtilities::MurmurHash64(ReinterpretCastRef<uint64>(ObjectPtr));
	}

public:
	template<typename T>
	FORCEINLINE bool CanBeCastedTo_Unsafe() const
	{
		const UObject* Object = Resolve();
		return !Object || Object->IsA<T>();
	}
	template<typename T>
	FORCEINLINE bool CanBeCastedTo() const
	{
		checkVoxelSlow(IsInGameThread());
		return CanBeCastedTo_Unsafe<T>();
	}

private:
	int32 ObjectIndex = 0;
	int32 ObjectSerialNumber = 0;
};
checkStatic(UE::Core::Private::InvalidWeakObjectIndex == 0);

template<typename ObjectType>
class TVoxelObjectPtr : public FVoxelObjectPtr
{
public:
	TVoxelObjectPtr() = default;

	FORCEINLINE TVoxelObjectPtr(ObjectType* Object)
		: FVoxelObjectPtr(Object)
	{
	}
	FORCEINLINE TVoxelObjectPtr(ObjectType& Object)
		: FVoxelObjectPtr(Object)
	{
	}
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ChildType> || std::is_const_v<ObjectType>)
	)
	FORCEINLINE TVoxelObjectPtr(const TObjectPtr<ChildType> Object)
		: FVoxelObjectPtr(Object.Get())
	{
	}
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ChildType> || std::is_const_v<ObjectType>)
	)
	FORCEINLINE TVoxelObjectPtr(const TWeakObjectPtr<ChildType> Object)
		: FVoxelObjectPtr(ReinterpretCastRef_Unaligned<FVoxelObjectPtr>(Object))
	{
	}
	FORCEINLINE TVoxelObjectPtr(decltype(nullptr))
	{
	}

public:
	FORCEINLINE ObjectType* Resolve() const
	{
		// Don't require including the type
		return (ObjectType*)FVoxelObjectPtr::Resolve();
	}
	FORCEINLINE ObjectType* Resolve_Ensured() const
	{
		ObjectType* Object = Resolve();
		ensure(Object || IsExplicitlyNull());
		return Object;
	}

public:
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ObjectType> || std::is_const_v<ChildType>)
	)
	FORCEINLINE TVoxelObjectPtr<ChildType> CastTo_Unsafe() const
	{
		if (!CanBeCastedTo_Unsafe<ChildType>())
		{
			return {};
		}

		return ReinterpretCastRef<TVoxelObjectPtr<ChildType>>(*this);
	}
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ObjectType> || std::is_const_v<ChildType>)
	)
	FORCEINLINE TVoxelObjectPtr<ChildType> CastTo() const
	{
		checkVoxelSlow(IsInGameThread());
		return CastTo_Unsafe<ChildType>();
	}
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ObjectType> || std::is_const_v<ChildType>)
	)
	FORCEINLINE TVoxelObjectPtr<ChildType> CastToChecked() const
	{
		checkVoxelSlow(CanBeCastedTo_Unsafe<ChildType>());
		return ReinterpretCastRef<TVoxelObjectPtr<ChildType>>(*this);
	}

public:
	FORCEINLINE operator TVoxelObjectPtr<const ObjectType>() const
	{
		return ReinterpretCastRef<TVoxelObjectPtr<const ObjectType>>(*this);
	}
	template<typename ParentType>
	requires
	(
		std::derived_from<ObjectType, ParentType> &&
		(!std::is_const_v<ObjectType> || std::is_const_v<ParentType>)
	)
	FORCEINLINE operator TVoxelObjectPtr<ParentType>() const
	{
		return ReinterpretCastRef<TVoxelObjectPtr<ParentType>>(*this);
	}

public:
	FORCEINLINE bool operator==(const FVoxelObjectPtr& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef<uint64>(Other);
	}
	template<typename OtherType, typename = std::enable_if_t<std::derived_from<OtherType, ObjectType> || std::derived_from<ObjectType, OtherType>>>
	FORCEINLINE bool operator==(const TVoxelObjectPtr<OtherType>& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef<uint64>(Other);
	}
	template<typename OtherType, typename = std::enable_if_t<std::derived_from<OtherType, ObjectType> || std::derived_from<ObjectType, OtherType>>>
	FORCEINLINE bool operator==(const TWeakObjectPtr<OtherType>& Other) const
	{
		return ReinterpretCastRef<uint64>(*this) == ReinterpretCastRef_Unaligned<uint64>(Other);
	}
	FORCEINLINE bool operator==(const UObject* Other) const
	{
		return *this == FVoxelObjectPtr(Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelObjectPtr& ObjectPtr)
	{
		return FVoxelUtilities::MurmurHash64(ReinterpretCastRef<uint64>(ObjectPtr));
	}
};

template<typename T>
FORCEINLINE TVoxelObjectPtr<T> MakeVoxelObjectPtr(T* Object)
{
	return TVoxelObjectPtr<T>(Object);
}
template<typename T>
FORCEINLINE TVoxelObjectPtr<T> MakeVoxelObjectPtr(T& Object)
{
	return TVoxelObjectPtr<T>(Object);
}
template<typename T>
FORCEINLINE TVoxelObjectPtr<T> MakeVoxelObjectPtr(const TObjectPtr<T>& Object)
{
	return TVoxelObjectPtr<T>(Object);
}

template<>
struct TIsZeroConstructType<FVoxelObjectPtr>
{
	static const bool Value = true;
};

template<typename T>
struct TIsZeroConstructType<TVoxelObjectPtr<T>>
{
	static const bool Value = true;
};

template<typename T>
struct TVoxelConstCast<TVoxelObjectPtr<const T>>
{
	FORCEINLINE static TVoxelObjectPtr<T>& ConstCast(TVoxelObjectPtr<const T>& Value)
	{
		return ReinterpretCastRef<TVoxelObjectPtr<T>>(Value);
	}
};

template<typename T>
struct TVoxelConstCast<const TVoxelObjectPtr<const T>>
{
	FORCEINLINE static const TVoxelObjectPtr<T>& ConstCast(const TVoxelObjectPtr<const T>& Value)
	{
		return ReinterpretCastRef<TVoxelObjectPtr<T>>(Value);
	}
};