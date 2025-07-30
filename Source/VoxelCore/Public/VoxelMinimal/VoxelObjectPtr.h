// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelObjectHelpers.h"

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
	FORCEINLINE FVoxelObjectPtr(decltype(nullptr))
	{
	}

public:
	UObject* Resolve() const;
	UObject* Resolve_Unsafe() const;
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
		checkVoxelSlow((ObjectIndex == 0) == (Class == nullptr));
		return ObjectIndex == 0;
	}
	FORCEINLINE UClass* GetClass() const
	{
		return Class;
	}

	FORCEINLINE bool operator==(const FVoxelObjectPtr& Other) const
	{
		checkVoxelSlow(RawValue != Other.RawValue || Class == Other.Class);
		return RawValue == Other.RawValue;
	}
	FORCEINLINE bool operator==(const UObject* Other) const
	{
		return *this == FVoxelObjectPtr(Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelObjectPtr& ObjectPtr)
	{
		return ObjectPtr.ObjectIndex;
	}

public:
	FORCEINLINE bool CanBeCastedTo(const UClass* OtherClass) const
	{
		if (IsExplicitlyNull())
		{
			return true;
		}

		return Class->IsChildOf(OtherClass);
	}
	template<typename T>
	FORCEINLINE bool CanBeCastedTo() const
	{
		return this->CanBeCastedTo(StaticClassFast<T>());
	}

private:
	union
	{
		struct
		{
			int32 ObjectIndex;
			int32 ObjectSerialNumber;
		};

		uint64 RawValue = 0;
	};
	UClass* Class = nullptr;
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
	FORCEINLINE TVoxelObjectPtr(decltype(nullptr))
	{
	}

public:
	FORCEINLINE ObjectType* Resolve() const
	{
		// Don't require including the type
		return reinterpret_cast<ObjectType*>(FVoxelObjectPtr::Resolve());
	}
	FORCEINLINE ObjectType* Resolve_Unsafe() const
	{
		// Don't require including the type
		return reinterpret_cast<ObjectType*>(FVoxelObjectPtr::Resolve_Unsafe());
	}
	FORCEINLINE ObjectType* Resolve_Ensured() const
	{
		ObjectType* Object = Resolve();
		ensureVoxelSlow(Object || IsExplicitlyNull());
		return Object;
	}

public:
	template<typename ChildType>
	requires
	(
		std::derived_from<ChildType, ObjectType> &&
		(!std::is_const_v<ObjectType> || std::is_const_v<ChildType>)
	)
	FORCEINLINE TVoxelObjectPtr<ChildType> CastTo() const
	{
		if (!CanBeCastedTo<ChildType>())
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
	FORCEINLINE TVoxelObjectPtr<ChildType> CastToChecked() const
	{
		checkVoxelSlow(CanBeCastedTo<ChildType>());
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
		return FVoxelObjectPtr::operator==(Other);
	}
	template<typename OtherType>
	requires
	(
		std::derived_from<OtherType, ObjectType> ||
		std::derived_from<ObjectType, OtherType>
	)
	FORCEINLINE bool operator==(const TVoxelObjectPtr<OtherType>& Other) const
	{
		return FVoxelObjectPtr::operator==(Other);
	}
	FORCEINLINE bool operator==(const UObject* Other) const
	{
		return FVoxelObjectPtr::operator==(Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelObjectPtr& ObjectPtr)
	{
		return GetTypeHash(static_cast<const FVoxelObjectPtr&>(ObjectPtr));
	}
};

template<typename T>
requires std::derived_from<T, UObject>
FORCEINLINE TVoxelObjectPtr<T> MakeVoxelObjectPtr(T* Object)
{
	return TVoxelObjectPtr<T>(Object);
}
template<typename T>
requires std::derived_from<T, UObject>
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