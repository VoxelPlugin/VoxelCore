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

	template<typename OtherType, typename = std::enable_if_t<TIsDerivedFrom<OtherType, T>::Value>>
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FORCEINLINE TSharedRef<T> MakeVoxelShareable(T* Object)
{
	FVoxelMemory::CheckIsVoxelAlloc(Object);
	return TSharedRef<T>(Object, TVoxelMemoryDeleter<T>());
}

// Need TEnableIf as &&& is equivalent to &, so T could get matched with Smthg&
template<typename T>
FORCEINLINE std::enable_if_t<!TIsReferenceType<T>::Value, TSharedRef<T>> MakeSharedCopy(T&& Data)
{
	return MakeShared<T>(MoveTemp(Data));
}
template<typename T>
FORCEINLINE std::enable_if_t<!TIsReferenceType<T>::Value, TUniquePtr<T>> MakeUniqueCopy(T&& Data)
{
	return MakeUnique<T>(MoveTemp(Data));
}

template<typename T>
FORCEINLINE TSharedRef<T> MakeSharedCopy(const T& Data)
{
	return MakeShared<T>(Data);
}
template<typename T>
FORCEINLINE TUniquePtr<T> MakeUniqueCopy(const T& Data)
{
	return MakeUnique<T>(Data);
}