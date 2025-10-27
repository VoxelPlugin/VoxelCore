// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"

template<typename Type>
struct TVoxelRefCountPtr
{
public:
	using ElementType = Type;

public:
	FORCEINLINE TVoxelRefCountPtr() = default;

	FORCEINLINE TVoxelRefCountPtr(Type* Reference)
		: PrivateReference(Reference)
	{
		if (PrivateReference)
		{
			PrivateReference->AddRef();
		}
	}
	FORCEINLINE TVoxelRefCountPtr(const TVoxelRefCountPtr& Other)
		: PrivateReference(Other.Get())
	{
		if (PrivateReference)
		{
			PrivateReference->AddRef();
		}
	}
	FORCEINLINE TVoxelRefCountPtr(TVoxelRefCountPtr&& Other)
		: PrivateReference(Other.Get())
	{
		Other.PrivateReference = nullptr;
	}

	template<typename OtherType>
	requires std::is_convertible_v<OtherType*, Type*>
	FORCEINLINE TVoxelRefCountPtr(const TVoxelRefCountPtr<OtherType>& Other)
		: TVoxelRefCountPtr(ReinterpretCastRef<TVoxelRefCountPtr>(Other))
	{
	}
	template<typename OtherType>
	requires (std::derived_from<Type, OtherType> && !std::is_convertible_v<OtherType*, Type*>)
	FORCEINLINE explicit TVoxelRefCountPtr(const TVoxelRefCountPtr<OtherType>& Other)
		: TVoxelRefCountPtr(ReinterpretCastRef<TVoxelRefCountPtr>(Other))
	{
	}

	template<typename OtherType>
	requires std::is_convertible_v<OtherType*, Type*>
	FORCEINLINE TVoxelRefCountPtr(TVoxelRefCountPtr<OtherType>&& Other)
		: TVoxelRefCountPtr(ReinterpretCastRef<TVoxelRefCountPtr>(MoveTemp(Other)))
	{
	}
	template<typename OtherType>
	requires (std::derived_from<Type, OtherType> && !std::is_convertible_v<OtherType*, Type*>)
	FORCEINLINE explicit TVoxelRefCountPtr(TVoxelRefCountPtr<OtherType>&& Other)
		: TVoxelRefCountPtr(ReinterpretCastRef<TVoxelRefCountPtr>(MoveTemp(Other)))
	{
	}

	FORCEINLINE ~TVoxelRefCountPtr()
	{
		if (PrivateReference)
		{
			PrivateReference->Release();
		}
	}

	FORCEINLINE TVoxelRefCountPtr& operator=(Type* Other)
	{
		// AddRef before Release in case it's the same ref count
		if (Other)
		{
			Other->AddRef();
		}
		if (PrivateReference)
		{
			PrivateReference->Release();
		}

		PrivateReference = Other;
		return *this;
	}
	FORCEINLINE TVoxelRefCountPtr& operator=(const TVoxelRefCountPtr& Other)
	{
		return this->operator=(Other.Get());
	}
	FORCEINLINE TVoxelRefCountPtr& operator=(TVoxelRefCountPtr&& Other)
	{
		checkVoxelSlow(this != &Other);

		if (PrivateReference)
		{
			PrivateReference->Release();
		}

		PrivateReference = Other.Get();
		Other.PrivateReference = nullptr;

		return *this;
	}

	template<typename OtherType>
	requires std::is_convertible_v<OtherType*, Type*>
	FORCEINLINE TVoxelRefCountPtr& operator=(const TVoxelRefCountPtr<OtherType>& Other)
	{
		return this->operator=(ReinterpretCastRef<TVoxelRefCountPtr>(Other));
	}

	template<typename OtherType>
	requires std::is_convertible_v<OtherType*, Type*>
	FORCEINLINE TVoxelRefCountPtr& operator=(TVoxelRefCountPtr<OtherType>&& Other)
	{
		return this->operator=(ReinterpretCastRef<TVoxelRefCountPtr>(MoveTemp(Other)));
	}

public:
	FORCEINLINE bool IsValid() const
	{
		return PrivateReference != nullptr;
	}
	FORCEINLINE int32 GetRefCount() const
	{
		if (!PrivateReference)
		{
			return 0;
		}
		return PrivateReference->GetRefCount();
	}
	FORCEINLINE Type* Get() const
	{
		if (PrivateReference)
		{
			checkVoxelSlow(PrivateReference->GetRefCount() > 0);
			checkVoxelSlow(PrivateReference->GetRefCount() < 1024 * 1024 * 1024);
		}

		return PrivateReference;
	}

public:
	FORCEINLINE Type* operator->() const
	{
		checkVoxelSlow(PrivateReference);
		checkVoxelSlow(PrivateReference->GetRefCount() > 0);
		checkVoxelSlow(PrivateReference->GetRefCount() < 1024 * 1024 * 1024);
		return PrivateReference;
	}
	FORCEINLINE Type& operator*() const
	{
		checkVoxelSlow(PrivateReference);
		checkVoxelSlow(PrivateReference->GetRefCount() > 0);
		checkVoxelSlow(PrivateReference->GetRefCount() < 1024 * 1024 * 1024);
		return *PrivateReference;
	}

	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(Type* Other) const
	{
		return Get() == Other;
	}
	FORCEINLINE bool operator==(const TVoxelRefCountPtr& Other) const
	{
		return Get() == Other.Get();
	}

private:
	Type* PrivateReference = nullptr;

	template <typename OtherType>
	friend struct TVoxelRefCountPtr;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
constexpr bool TIsTVoxelRefCountPtr_V = false;

template<typename T>
constexpr bool TIsTVoxelRefCountPtr_V<TVoxelRefCountPtr<T>> = true;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_DEBUG
extern VOXELCORE_API FVoxelCounter32 GNumVoxelRefCountThis;
#endif

template<typename Type>
class TVoxelRefCountThis
{
public:
	FORCEINLINE TVoxelRefCountThis()
	{
#if VOXEL_DEBUG
		GNumVoxelRefCountThis.Increment();
#endif
	}

	TVoxelRefCountThis(const TVoxelRefCountThis&) = delete;
	TVoxelRefCountThis& operator=(const TVoxelRefCountThis&) = delete;

#if VOXEL_DEBUG
	FORCEINLINE ~TVoxelRefCountThis()
	{
		CheckHasValidRefCount();

		ReinterpretCastRef<int32>(RefCount) = -1;

		GNumVoxelRefCountThis.Decrement();
	}
#endif

	FORCEINLINE void AddRef() const
	{
		CheckHasValidRefCount();

		RefCount.Increment();
	}

	FORCEINLINE void Release() const
	{
		CheckHasValidRefCount();

		if (RefCount.Decrement_ReturnNew() == 0)
		{
			delete static_cast<const Type*>(this);
		}
	}

	FORCEINLINE bool IsUnique() const
	{
		CheckHasValidRefCount();
		return RefCount.Get() == 1;
	}
	FORCEINLINE int32 GetRefCount() const
	{
		CheckHasValidRefCount();
		return RefCount.Get();
	}

	FORCEINLINE void CheckHasValidRefCount() const
	{
		checkVoxelSlow(RefCount.Get() >= 0);
		checkVoxelSlow(RefCount.Get() < 1024 * 1024 * 1024);
	}

private:
	mutable FVoxelCounter32 RefCount;
};