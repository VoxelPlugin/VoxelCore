// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Containers/VoxelBitArrayView.h"

template<typename Allocator>
class TVoxelBitArray
{
public:
	checkStatic(std::is_same_v<typename Allocator::SizeType, int32>);

	static constexpr int32 NumBitsPerWord = FConstVoxelBitArrayView::NumBitsPerWord;
	static constexpr int32 NumBitsPerWord_Log2 = FConstVoxelBitArrayView::NumBitsPerWord_Log2;
	static constexpr uint64 WordMask = FConstVoxelBitArrayView::WordMask;
	static constexpr uint64 EmptyWord = FConstVoxelBitArrayView::EmptyWord;
	static constexpr uint64 FullWord = FConstVoxelBitArrayView::FullWord;

public:
	TVoxelBitArray() = default;

	FORCEINLINE TVoxelBitArray(TVoxelBitArray&& Other)
	{
		*this = MoveTemp(Other);
	}
	FORCEINLINE TVoxelBitArray(const TVoxelBitArray& Other)
	{
		*this = Other;
	}

	FORCEINLINE TVoxelBitArray& operator=(TVoxelBitArray&& Other)
	{
		checkVoxelSlow(this != &Other);

		AllocatorInstance.MoveToEmpty(Other.AllocatorInstance);

		NumBits = Other.NumBits;
		MaxBits = Other.MaxBits;
		Other.NumBits = 0;
		Other.MaxBits = 0;

		EnsurePartialSlackBitsCleared();

		return *this;
	}
	FORCEINLINE TVoxelBitArray& operator=(const TVoxelBitArray& Other)
	{
		checkVoxelSlow(this != &Other);

		NumBits = Other.NumBits;
		SetMaxBits(Other.NumBits);

		if (NumWords() != 0)
		{
			FVoxelUtilities::Memcpy(GetWordView(), Other.GetWordView());
		}

		EnsurePartialSlackBitsCleared();

		return *this;
	}

public:
	template<typename OtherAllocator>
	FORCEINLINE bool operator==(const TVoxelBitArray<OtherAllocator>& Other) const
	{
		if (Num() != Other.Num())
		{
			return false;
		}

		return FVoxelUtilities::Equal(GetWordView(), Other.GetWordView());
	}

public:
	FORCEINLINE void Reserve(const int32 NewMaxBits)
	{
		if (NewMaxBits <= MaxBits)
		{
			return;
		}

		SetMaxBits(NewMaxBits);
	}
	FORCEINLINE void Empty(const int32 NewMaxBits = 0)
	{
		NumBits = 0;
		SetMaxBits(NewMaxBits);
	}
	FORCEINLINE void Shrink()
	{
		SetMaxBits(NumBits);
	}
	FORCEINLINE void Reset()
	{
		NumBits = 0;
	}

	// No SetNumUninitialized: the last word must always be zero-padded
	void SetNum(const int32 NewNumBits, const bool bValue)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewNumBits, 128);

		const int32 OldNumBits = NumBits;

		NumBits = NewNumBits;
		this->SetMaxBits(FMath::Max(MaxBits, NewNumBits));

		if (NewNumBits > OldNumBits)
		{
			this->SetRange(OldNumBits, NewNumBits - OldNumBits, bValue);
		}

		ClearPartialSlackBits();
	}

public:
	void BitwiseOr(const TVoxelBitArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 128);
		checkVoxelSlow(Num() == Other.Num());

		EnsurePartialSlackBitsCleared();
		Other.EnsurePartialSlackBitsCleared();

		for (int32 WordIndex = 0; WordIndex < NumWords(); WordIndex++)
		{
			GetWord(WordIndex) |= Other.GetWord(WordIndex);
		}
	}
	void BitwiseAnd(const TVoxelBitArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 128);
		checkVoxelSlow(Num() == Other.Num());

		EnsurePartialSlackBitsCleared();
		Other.EnsurePartialSlackBitsCleared();

		for (int32 WordIndex = 0; WordIndex < NumWords(); WordIndex++)
		{
			GetWord(WordIndex) &= Other.GetWord(WordIndex);
		}
	}

public:
	FVoxelBitArrayView View()
	{
		return FVoxelBitArrayView(GetWordData(), Num());
	}
	FConstVoxelBitArrayView View() const
	{
		return FConstVoxelBitArrayView(GetWordData(), Num());
	}

public:
	FORCEINLINE void SetRange(
		const int32 StartIndex,
		const int32 NumToSet,
		const bool bValue)
	{
		View().SetRange(StartIndex, NumToSet, bValue);
	}

public:
	FORCEINLINE TVoxelOptional<bool> TryGetAll() const
	{
		return View().TryGetAll();
	}
	FORCEINLINE bool AllEqual(const bool bValue) const
	{
		return View().AllEqual(bValue);
	}
	FORCEINLINE int32 CountSetBits() const
	{
		return View().CountSetBits();
	}

public:
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return NumWords() * sizeof(uint64);
	}
	FORCEINLINE int32 Num() const
	{
		return NumBits;
	}
	FORCEINLINE int32 NumWords() const
	{
		return FVoxelUtilities::DivideCeil_Positive_Log2(NumBits, NumBitsPerWord_Log2);
	}

public:
	FORCEINLINE int32 AddUninitialized(const int32 NumBitsToAdd = 1)
	{
		checkVoxelSlow(NumBitsToAdd >= 0);

		const int32 OldNumBits = NumBits;

		const int32 OldLastWordIndex = NumBits == 0 ? -1 : (NumBits - 1) >> NumBitsPerWord_Log2;
		const int32 NewLastWordIndex = (NumBits + NumBitsToAdd - 1) >> NumBitsPerWord_Log2;
		if (NewLastWordIndex == OldLastWordIndex)
		{
			NumBits += NumBitsToAdd;
			EnsurePartialSlackBitsCleared();
			return OldNumBits;
		}

		this->Reserve(NumBits + NumBitsToAdd);

		NumBits += NumBitsToAdd;
		ClearPartialSlackBits();

		return OldNumBits;
	}
	FORCEINLINE int32 Add(const bool bValue)
	{
		const int32 Index = AddUninitialized(1);
		View()[Index] = bValue;
		return Index;
	}

	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < NumBits;
	}

	FORCEINLINE FVoxelBitReference operator[](const int32 Index)
	{
		return View()[Index];
	}
	FORCEINLINE FVoxelConstBitReference operator[](const int32 Index) const
	{
		return View()[Index];
	}

	FORCEINLINE FVoxelSetBitIterator IterateSetBits() const
	{
		return View().IterateSetBits();
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue)
	{
		return View().AtomicSet_ReturnOld(Index, bValue);
	}
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue)
	{
		View().AtomicSet(Index, bValue);
	}

public:
	FORCEINLINE uint64* GetWordData()
	{
		return static_cast<uint64*>(AllocatorInstance.GetAllocation());
	}
	FORCEINLINE const uint64* GetWordData() const
	{
		return static_cast<uint64*>(AllocatorInstance.GetAllocation());
	}

	FORCEINLINE TVoxelArrayView<uint64> GetWordView()
	{
		return { GetWordData(), NumWords() };
	}
	FORCEINLINE TConstVoxelArrayView<uint64> GetWordView() const
	{
		return { GetWordData(), NumWords() };
	}

	FORCEINLINE uint64& GetWord(const int32 Index)
	{
		return GetWordView()[Index];
	}
	FORCEINLINE const uint64& GetWord(const int32 Index) const
	{
		return GetWordView()[Index];
	}

private:
	using AllocatorType = typename Allocator::template ForElementType<uint64>;

	AllocatorType AllocatorInstance;
	int32 NumBits = 0;
	int32 MaxBits = 0;

	FORCENOINLINE void SetMaxBits(const int32 NewMaxBits)
	{
		if (MaxBits == NewMaxBits)
		{
			return;
		}

		const int32 PreviousMaxWords = FVoxelUtilities::DivideCeil_Positive_Log2(MaxBits, NumBitsPerWord_Log2);
		const int32 NewMaxWords = FVoxelUtilities::DivideCeil_Positive_Log2(NewMaxBits, NumBitsPerWord_Log2);

		AllocatorInstance.ResizeAllocation(PreviousMaxWords, NewMaxWords, sizeof(uint64));

		MaxBits = NewMaxBits;
	}
	FORCEINLINE void ClearPartialSlackBits()
	{
		// TBitArray has a contract about bits outside of the active range - the bits in the final word past NumBits are guaranteed to be 0
		// This prevents easy-to-make determinism errors from users of TBitArray that do not carefully mask the final word.
		// It also allows us optimize some operations which would otherwise require us to mask the last word.

		const int32 UsedBits = NumBits % NumBitsPerWord;
		if (UsedBits == 0)
		{
			return;
		}

		const int32 LastWordIndex = NumBits / NumBitsPerWord;
		const uint64 SlackMask = FullWord >> (NumBitsPerWord - UsedBits);

		GetWord(LastWordIndex) &= SlackMask;
	}
	FORCEINLINE void EnsurePartialSlackBitsCleared() const
	{
#if VOXEL_DEBUG
		const int32 UsedBits = NumBits % NumBitsPerWord;
		if (UsedBits == 0)
		{
			return;
		}

		const int32 LastWordIndex = NumBits / NumBitsPerWord;
		const uint64 SlackMask = FullWord >> (NumBitsPerWord - UsedBits);

		ensure((GetWord(LastWordIndex) & ~SlackMask) == 0);
#endif
	}

	template<typename>
	friend class TVoxelBitArray;
};

template<typename Allocator>
uint64* RESTRICT GetData(TVoxelBitArray<Allocator>& Array)
{
	return Array.GetWordData();
}
template<typename Allocator>
const uint64* RESTRICT GetData(const TVoxelBitArray<Allocator>& Array)
{
	return Array.GetWordData();
}

// Can have different meanings
//template<typename Allocator>
//auto GetNum(const TVoxelBitArray<Allocator>& Array)
//{
//	return Array.Num();
//}

using FVoxelBitArray = TVoxelBitArray<FDefaultAllocator>;

template<int32 NumInlineElements>
using TVoxelInlineBitArray = TVoxelBitArray<TInlineAllocator<FMath::DivideAndRoundUp(NumInlineElements, 32)>>;

template<int32 MaxNumElements>
using TVoxelFixedBitArray = TVoxelBitArray<TFixedAllocator<FMath::DivideAndRoundUp(MaxNumElements, 32)>>;