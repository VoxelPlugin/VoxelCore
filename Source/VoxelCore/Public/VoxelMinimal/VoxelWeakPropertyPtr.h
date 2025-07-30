// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelObjectPtr.h"

template<typename T>
struct TVoxelWeakPropertyPtr
{
public:
	using ObjectType = std::conditional_t<std::is_const_v<T>, const UObject, UObject>;

	TVoxelWeakPropertyPtr() = default;
	FORCEINLINE TVoxelWeakPropertyPtr(
		ObjectType& Object,
		T& Property)
	{
		checkVoxelSlow(reinterpret_cast<const uint8*>(&Object) <= reinterpret_cast<const uint8*>(&Property));
		checkVoxelSlow(reinterpret_cast<const uint8*>(&Property) - reinterpret_cast<const uint8*>(&Object) <= 2048);

		WeakObject = &Object;
		PropertyPtr = &Property;
	}

	template<typename OtherType>
	requires
	(
		!std::is_same_v<T, OtherType> &&
		std::derived_from<OtherType, T>
	)
	FORCEINLINE TVoxelWeakPropertyPtr(const TVoxelWeakPropertyPtr<OtherType>& Other)
	{
		ObjectType* Object = Other.WeakObject.Get();
		if (!Object)
		{
			return;
		}

		*this = TVoxelWeakPropertyPtr(*Object, *Other.PropertyPtr);
	}

	FORCEINLINE bool IsValid() const
	{
		return WeakObject.IsValid();
	}
	FORCEINLINE T* Get() const
	{
		return IsValid() ? PropertyPtr : nullptr;
	}

	FORCEINLINE T* operator->() const
	{
		checkVoxelSlow(IsValid());
		return PropertyPtr;
	}
	FORCEINLINE T& operator*() const
	{
		checkVoxelSlow(IsValid());
		return *PropertyPtr;
	}

	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(const TVoxelWeakPropertyPtr& Other) const
	{
		return
			WeakObject == Other.WeakObject &&
			PropertyPtr == Other.PropertyPtr;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelWeakPropertyPtr& Key)
	{
		return
			GetTypeHash(Key.WeakObject) ^
			GetTypeHash(Key.PropertyPtr);
	}

private:
	TVoxelObjectPtr<ObjectType> WeakObject;
	T* PropertyPtr = nullptr;

	template<typename>
	friend struct TVoxelWeakPropertyPtr;
};

template<typename ObjectType, typename T>
FORCEINLINE TVoxelWeakPropertyPtr<T> MakeVoxelWeakPropertyPtr(ObjectType* Object, T& Property)
{
	checkVoxelSlow(Object);
	return TVoxelWeakPropertyPtr<T>(*Object, Property);
}