// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Containers/SparseArray.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelBitArrayHelpers.h"

template<typename InElementType, typename IndexType = int32, typename Allocator = FVoxelSparseArrayAllocator>
class TVoxelSparseArray : public TSparseArray<InElementType, Allocator>
{
public:
	using Super = TSparseArray<InElementType, Allocator>;
	using Super::Super;
	using ElementType = InElementType;

	FORCEINLINE ElementType& operator[](IndexType Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return Super::operator[](int32(Index));
	}
	FORCEINLINE const ElementType& operator[](IndexType Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return Super::operator[](int32(Index));
	}
	FORCEINLINE bool IsValidIndex(IndexType Index) const
	{
		return Super::IsValidIndex(int32(Index));
	}

	FORCEINLINE bool IsAllocated(const int32 Index) const
	{
		checkVoxelSlow(GetArray().AllocationFlags.IsValidIndex(Index));
		return FVoxelBitArrayHelpers::Get(GetArray().AllocationFlags.GetData(), Index);
	}
	FORCEINLINE IndexType MakeIndex(const int32 Index) const
	{
		checkVoxelSlow(IsAllocated(Index));
		return IndexType(Index);
	}

	FORCEINLINE IndexType Add(const ElementType& Element)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new (Allocation.Pointer) ElementType(Element);
		return IndexType(Allocation.Index);
	}
	FORCEINLINE IndexType Add(ElementType&& Element)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new (Allocation.Pointer) ElementType(MoveTemp(Element));
		return IndexType(Allocation.Index);
	}
	FORCEINLINE void RemoveAt(IndexType Index)
	{
		Super::RemoveAt(int32(Index));
	}

	FORCEINLINE FSparseArrayAllocationInfo AddUninitialized()
	{
		return GetArray().AddUninitialized();
	}
	void Reserve(int32 ExpectedNumElements)
	{
		if (GetArray().Data.Num() == 0 &&
			ExpectedNumElements > 2)
		{
			GetArray().Reserve(ExpectedNumElements);
			return;
		}

		Super::Reserve(ExpectedNumElements);
	}

private:
	struct FArray
	{
		using FElementOrFreeListLink = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(ElementType), alignof(ElementType)>>;
		using DataType = TVoxelArray<FElementOrFreeListLink, typename Allocator::ElementAllocator>;
		using AllocationBitArrayType = TBitArray<typename Allocator::BitArrayAllocator>;

		DataType Data;
		AllocationBitArrayType AllocationFlags;
		int32 FirstFreeIndex;
		int32 NumFreeIndices;

		FORCEINLINE FSparseArrayAllocationInfo AllocateIndex(int32 Index)
		{
			checkVoxelSlow(Data.IsValidIndex(Index));
			checkVoxelSlow(!AllocationFlags[Index]);

			FVoxelBitArrayHelpers::Set(AllocationFlags.GetData(), Index, true);

			FSparseArrayAllocationInfo Result;
			Result.Index = Index;
			Result.Pointer = &Data[Index].ElementData;
			return Result;
		}
		FORCEINLINE FSparseArrayAllocationInfo AddUninitialized()
		{
			if (NumFreeIndices == 0)
			{
				// Will only happen if Reserve was too small
				const int32 Index = Data.AddUninitialized(1);
				AllocationFlags.Add(false);
				return AllocateIndex(Index);
			}

			checkVoxelSlow(NumFreeIndices > 0);
			const int32 Index = FirstFreeIndex;

			checkVoxelSlow(Data[FirstFreeIndex].PrevFreeIndex == -1);
			FirstFreeIndex = Data[FirstFreeIndex].NextFreeIndex;

			NumFreeIndices--;

			checkVoxelSlow((FirstFreeIndex == -1) == (NumFreeIndices == 0));
			if (FirstFreeIndex != -1)
			{
				Data[FirstFreeIndex].PrevFreeIndex = -1;
			}

			return AllocateIndex(Index);
		}
		FORCENOINLINE void Reserve(const int32 ExpectedNumElements)
		{
			checkVoxelSlow(ExpectedNumElements > 2);
			checkVoxelSlow(Data.Num() == 0);
			checkVoxelSlow(AllocationFlags.Num() == 0);
			checkVoxelSlow(FirstFreeIndex == -1);
			checkVoxelSlow(NumFreeIndices == 0);

			FVoxelUtilities::SetNumFast(Data, ExpectedNumElements);
			AllocationFlags.Init(false, ExpectedNumElements);

			FirstFreeIndex = 0;
			NumFreeIndices = ExpectedNumElements;

			for (int32 Index = 0; Index < ExpectedNumElements; Index++)
			{
				Data[Index].PrevFreeIndex = Index - 1;
				Data[Index].NextFreeIndex = Index + 1;
			}

			Data[0].PrevFreeIndex = -1;
			Data[ExpectedNumElements - 1].NextFreeIndex = -1;
		}
	};
	FORCEINLINE FArray& GetArray()
	{
		return ReinterpretCastRef<FArray>(*this);
	}
	FORCEINLINE const FArray& GetArray() const
	{
		return ReinterpretCastRef<const FArray>(*this);
	}
};

template<typename>
class TVoxelSparseArrayIndex
{
public:
	TVoxelSparseArrayIndex() = default;

	FORCEINLINE bool IsValid() const
	{
		return Index != -1;
	}
	FORCEINLINE int32 GetIndex() const
	{
		return Index;
	}

	FORCEINLINE bool operator==(const TVoxelSparseArrayIndex Other) const
	{
		return Other.Index == Index;
	}
	FORCEINLINE bool operator!=(const TVoxelSparseArrayIndex Other) const
	{
		return Other.Index != Index;
	}

	FORCEINLINE friend uint32 GetTypeHash(const TVoxelSparseArrayIndex Value)
	{
		return GetTypeHash(Value.Index);
	}

private:
	FORCEINLINE explicit TVoxelSparseArrayIndex(const int32 Index)
		: Index(Index)
	{
	}

	int32 Index = -1;

	FORCEINLINE operator int32() const
	{
		return Index;
	}

	template<typename, typename, typename>
	friend class TVoxelSparseArray;
};

#define DECLARE_VOXEL_SPARSE_INDEX(Name) using Name = TVoxelSparseArrayIndex<class _## Name ##_Unique>;