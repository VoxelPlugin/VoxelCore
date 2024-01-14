// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMacros.h"

#ifndef ENABLE_VOXEL_ALLOCATOR
#define ENABLE_VOXEL_ALLOCATOR 1
#endif

struct FVoxelMemory;

constexpr const FVoxelMemory* GVoxelMemory = nullptr;

void* operator new(size_t Size, const FVoxelMemory*);
void* operator new(size_t Size, std::align_val_t Alignment, const FVoxelMemory*);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelMemoryDeleter;

template<typename T>
using TVoxelUniquePtr = TUniquePtr<T, TVoxelMemoryDeleter<T>>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern VOXELCORE_API const uint32 GVoxelMemoryTLS;

struct VOXELCORE_API FVoxelMemory
{
public:
#if ENABLE_VOXEL_ALLOCATOR && VOXEL_DEBUG
	static void CheckIsVoxelAlloc(const void* Original);
#else
	FORCEINLINE static void CheckIsVoxelAlloc(const void* Original) {}
#endif

	static void* MallocImpl(SIZE_T Count, uint32 Alignment);
	static void* ReallocImpl(void* Original, SIZE_T OriginalCount, SIZE_T Count, uint32 Alignment);
	static void FreeImpl(void* Original);

public:
	FORCEINLINE static void* Malloc(const SIZE_T Count, const uint32 Alignment = DEFAULT_ALIGNMENT)
	{
#if ENABLE_VOXEL_ALLOCATOR
		return MallocImpl(Count, Alignment);
#else
		return FMemory::Malloc(Count, Alignment);
#endif
	}
	FORCEINLINE static void* Realloc(void* Original, const SIZE_T OriginalCount, const SIZE_T Count, const uint32 Alignment)
	{
#if ENABLE_VOXEL_ALLOCATOR
		return ReallocImpl(Original, OriginalCount, Count, Alignment);
#else
		return FMemory::Realloc(Original, Count, Alignment);
#endif
	}
	FORCEINLINE static void Free(void* Original)
	{
#if ENABLE_VOXEL_ALLOCATOR
		FreeImpl(Original);
#else
		FMemory::Free(Original);
#endif
	}

public:
	template<typename T>
	FORCEINLINE static void Delete(const T* Object)
	{
		if (!Object)
		{
			return;
		}

		Object->~T();
		FVoxelMemory::Free(ConstCast(Object));
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelMemoryDeleter
{
	TVoxelMemoryDeleter() = default;

	template<typename OtherType, typename = typename TEnableIf<TIsDerivedFrom<OtherType, T>::Value>::Type>
	TVoxelMemoryDeleter(const TVoxelMemoryDeleter<OtherType>&)
	{
	}

	FORCEINLINE void operator()(T* Object) const
	{
		FVoxelMemory::Delete(Object);
	}
};

FORCEINLINE void* operator new(const size_t Size, const FVoxelMemory*)
{
	return FVoxelMemory::Malloc(Size);
}
FORCEINLINE void* operator new(const size_t Size, std::align_val_t Alignment, const FVoxelMemory*)
{
	return FVoxelMemory::Malloc(Size, uint32(Alignment));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// See FContainerAllocatorInterface
template<int32 IndexSize>
class TVoxelAllocator
{
public:
	using SizeType = typename TBitsToSizeType<IndexSize>::Type;

	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	class ForAnyElementType
	{
	public:
		ForAnyElementType() = default;
		UE_NONCOPYABLE(ForAnyElementType);

		FORCEINLINE ~ForAnyElementType()
		{
			if (Data)
			{
				FVoxelMemory::Free(Data);
			}
		}

		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			this->MoveToEmptyFromOtherAllocator<TVoxelAllocator>(Other);
		}
		template<typename OtherAllocator>
		FORCEINLINE void MoveToEmptyFromOtherAllocator(typename OtherAllocator::ForAnyElementType& Other)
		{
			if (Data)
			{
				FVoxelMemory::Free(Data);
			}

			void*& OtherData = ReinterpretCastRef<void*>(Other);

			Data = OtherData;
			OtherData = nullptr;
		}

		FORCEINLINE void* GetAllocation() const
		{
			return Data;
		}
		FORCEINLINE void ResizeAllocation(const SizeType PreviousNumElements, const SizeType NumElements, const SIZE_T NumBytesPerElement)
		{
			this->ResizeAllocation(PreviousNumElements, NumElements, NumBytesPerElement, DEFAULT_ALIGNMENT);
		}
		FORCEINLINE void ResizeAllocation(const SizeType PreviousNumElements, const SizeType NumElements, const SIZE_T NumBytesPerElement, const uint32 AlignmentOfElement)
		{
			if (PreviousNumElements == 0)
			{
				if (Data)
				{
					FVoxelMemory::Free(Data);
					Data = nullptr;
				}
				if (NumElements != 0)
				{
					Data = FVoxelMemory::Malloc(NumElements * NumBytesPerElement, AlignmentOfElement);
				}
				return;
			}

			// Otherwise PreviousNumElements == 0
			checkVoxelSlow(Data);

			if (NumElements == 0)
			{
				FVoxelMemory::Free(Data);
				Data = nullptr;
				return;
			}

			Data = FVoxelMemory::Realloc(Data, PreviousNumElements * NumBytesPerElement, NumElements * NumBytesPerElement, AlignmentOfElement);
		}
		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackReserve(SizeType NumElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true, AlignmentOfElement);
		}
		FORCEINLINE SizeType CalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackShrink(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true, AlignmentOfElement);
		}
		FORCEINLINE SizeType CalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE SizeType CalculateSlackGrow(SizeType NumElements, SizeType NumAllocatedElements, SIZE_T NumBytesPerElement, uint32 AlignmentOfElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true, AlignmentOfElement);
		}

		FORCEINLINE SIZE_T GetAllocatedSize(SizeType NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}
		FORCEINLINE bool HasAllocation() const
		{
			return Data != nullptr;
		}
		FORCEINLINE SizeType GetInitialCapacity() const
		{
			return 0;
		}

	private:
		void* Data = nullptr;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:
		ForElementType() = default;

		FORCEINLINE ElementType* GetAllocation() const
		{
			return static_cast<ElementType*>(ForAnyElementType::GetAllocation());
		}
	};
};

template<uint8 IndexSize>
struct TAllocatorTraits<TVoxelAllocator<IndexSize>> : TAllocatorTraitsBase<TVoxelAllocator<IndexSize>>
{
	enum { IsZeroConstruct          = true };
	enum { SupportsElementAlignment = true };
};

template<uint8 FromIndexSize, uint8 ToIndexSize>
struct TCanMoveBetweenAllocators<TVoxelAllocator<FromIndexSize>, TVoxelAllocator<ToIndexSize>>
{
	enum { Value = true };
};

template<uint8 FromIndexSize, uint8 ToIndexSize>
struct TCanMoveBetweenAllocators<TVoxelAllocator<FromIndexSize>, TSizedHeapAllocator<ToIndexSize>>
{
	enum { Value = true };
};

template<uint8 FromIndexSize, uint8 ToIndexSize>
struct TCanMoveBetweenAllocators<TSizedHeapAllocator<FromIndexSize>, TVoxelAllocator<ToIndexSize>>
{
	enum { Value = true };
};

using FVoxelAllocator = TVoxelAllocator<32>;
using FVoxelAllocator64 = TVoxelAllocator<64>;

template<uint32 NumInlineElements>
using TVoxelInlineAllocator = TInlineAllocator<NumInlineElements, FVoxelAllocator>;

struct FVoxelSparseArrayAllocator
{
	using ElementAllocator = FVoxelAllocator;
	using BitArrayAllocator = FVoxelAllocator;
};

struct FVoxelSetAllocator
{
	using SparseArrayAllocator = FVoxelSparseArrayAllocator;
	using HashAllocator = TVoxelInlineAllocator<1>;

	FORCEINLINE static uint32 GetNumberOfHashBuckets(const uint32 NumHashedElements)
	{
		return TSetAllocator<>::GetNumberOfHashBuckets(NumHashedElements);
	}
};

template<typename T, typename KeyFuncs = DefaultKeyFuncs<T>>
using TVoxelSet = TSet<T, KeyFuncs, FVoxelSetAllocator>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE TSharedRef<T> MakeVoxelShareable(T* Object)
{
	FVoxelMemory::CheckIsVoxelAlloc(Object);
	return TSharedRef<T>(Object, TVoxelMemoryDeleter<T>());
}
template<typename T, typename... ArgsTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgsTypes...>>>
FORCEINLINE TSharedRef<T> MakeVoxelShared(ArgsTypes&&... Args)
{
	return MakeVoxelShareable(new (GVoxelMemory) T(Forward<ArgsTypes>(Args)...));
}

template<typename T, typename... ArgsTypes, typename = std::enable_if_t<std::is_constructible_v<T, ArgsTypes...>>>
FORCEINLINE TVoxelUniquePtr<T> MakeVoxelUnique(ArgsTypes&&... Args)
{
	return TVoxelUniquePtr<T>(new (GVoxelMemory) T(Forward<ArgsTypes>(Args)...));
}

// Need TEnableIf as &&& is equivalent to &, so T could get matched with Smthg&
template<typename T>
FORCEINLINE typename TEnableIf<!TIsReferenceType<T>::Value, TSharedRef<T>>::Type MakeSharedCopy(T&& Data)
{
	return MakeVoxelShared<T>(MoveTemp(Data));
}
template<typename T>
FORCEINLINE typename TEnableIf<!TIsReferenceType<T>::Value, TVoxelUniquePtr<T>>::Type MakeUniqueCopy(T&& Data)
{
	return MakeVoxelUnique<T>(MoveTemp(Data));
}

template<typename T>
FORCEINLINE TSharedRef<T> MakeSharedCopy(const T& Data)
{
	return MakeVoxelShared<T>(Data);
}
template<typename T>
FORCEINLINE TVoxelUniquePtr<T> MakeUniqueCopy(const T& Data)
{
	return MakeVoxelUnique<T>(Data);
}