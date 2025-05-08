// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Containers/VoxelBitArrayUtilities.h"

// Tricky: we need to ensure the last word is always zero-padded
template<typename Allocator>
class TVoxelBitArray
{
public:
	typedef typename Allocator::SizeType SizeType;

	static constexpr int32 NumBitsPerWord = FVoxelBitArrayUtilities::NumBitsPerWord;
	static constexpr int32 NumBitsPerWord_Log2 = FVoxelBitArrayUtilities::NumBitsPerWord_Log2;
	static constexpr uint32 IndexInWordMask = FVoxelBitArrayUtilities::IndexInWordMask;

	template<typename>
	friend class TVoxelBitArray;

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
	friend FArchive& operator<<(FArchive& Ar, TVoxelBitArray& BitArray)
	{
		Ar << BitArray.NumBits;

		if (Ar.IsLoading())
		{
			BitArray.SetMaxBits(BitArray.NumBits);
		}

		BitArray.GetWordView().Serialize(Ar);
		BitArray.EnsurePartialSlackBitsCleared();

		return Ar;
	}
	FORCEINLINE void Reserve(SizeType NewMaxBits)
	{
		if (NewMaxBits <= MaxBits)
		{
			return;
		}

		SetMaxBits(NewMaxBits);
	}
	FORCEINLINE void Empty(SizeType NewMaxBits = 0)
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
	void SetNum(SizeType NewNumBits, const bool bValue)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewNumBits, 128);

		const SizeType OldNumBits = NumBits;

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
	FORCEINLINE void SetRange(SizeType Index, SizeType Num, bool Value)
	{
		FVoxelBitArrayUtilities::SetRange(GetWordView(), Index, Num, Value);
	}

public:
	FORCEINLINE TOptional<bool> TryGetAll() const
	{
		return FVoxelBitArrayUtilities::TryGetAll(GetWordView(), Num());
	}
	FORCEINLINE bool AllEqual(bool bValue) const
	{
		return FVoxelBitArrayUtilities::AllEqual(GetWordView(), Num(), bValue);
	}
	FORCEINLINE SizeType CountSetBits() const
	{
		EnsurePartialSlackBitsCleared();
		return FVoxelUtilities::CountSetBits(GetWordView());
	}
	FORCEINLINE SizeType CountSetBits(SizeType Count) const
	{
		checkVoxelSlow(Count <= Num());
		return FVoxelUtilities::CountSetBits(GetWordView(), Count);
	}

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterate>>>
	FORCEINLINE EVoxelIterate ForAllSetBits(LambdaType Lambda) const
	{
		return FVoxelBitArrayUtilities::ForAllSetBits(GetWordView(), Num(), Lambda);
	}

public:
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return FVoxelUtilities::DivideCeil_Positive_Log2(MaxBits, NumBitsPerWord_Log2) * sizeof(uint32);
	}
	FORCEINLINE SizeType Num() const
	{
		return NumBits;
	}
	FORCEINLINE SizeType NumWords() const
	{
		return FVoxelUtilities::DivideCeil_Positive_Log2(NumBits, NumBitsPerWord_Log2);
	}

public:
	FORCEINLINE int32 AddUninitialized(int32 NumBitsToAdd = 1)
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
		checkVoxelSlow(IsValidIndex(Index));
		FVoxelBitArrayUtilities::Set(GetWordView(), Index, bValue);
		return Index;
	}

	FORCEINLINE bool IsValidIndex(SizeType Index) const
	{
		return 0 <= Index && Index < NumBits;
	}

	FORCEINLINE FVoxelBitReference operator[](SizeType Index)
	{
		checkVoxelSlow(this->IsValidIndex(Index));
		return FVoxelBitReference(
			this->GetWord(Index >> NumBitsPerWord_Log2),
			1u << (Index & IndexInWordMask));
	}
	FORCEINLINE FVoxelConstBitReference operator[](SizeType Index) const
	{
		checkVoxelSlow(this->IsValidIndex(Index));
		return FVoxelConstBitReference(
			this->GetWord(Index >> NumBitsPerWord_Log2),
			1u << (Index & IndexInWordMask));
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue)
	{
		uint32& Word = this->GetWord(Index >> NumBitsPerWord_Log2);
		const uint32 Mask = 1u << (Index & IndexInWordMask);

		if (bValue)
		{
			return ReinterpretCastRef<TVoxelAtomic<uint32>>(Word).Or_ReturnOld(Mask) & Mask;
		}
		else
		{
			return ReinterpretCastRef<TVoxelAtomic<uint32>>(Word).And_ReturnOld(~Mask) & Mask;
		}
	}
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue)
	{
		(void)AtomicSet_ReturnOld(Index, bValue);
	}

public:
	FORCEINLINE uint32* GetWordData()
	{
		return static_cast<uint32*>(AllocatorInstance.GetAllocation());
	}
	FORCEINLINE const uint32* GetWordData() const
	{
		return static_cast<uint32*>(AllocatorInstance.GetAllocation());
	}

	FORCEINLINE TVoxelArrayView<uint32, SizeType> GetWordView()
	{
		return { GetWordData(), NumWords() };
	}
	FORCEINLINE TConstVoxelArrayView<uint32, SizeType> GetWordView() const
	{
		return { GetWordData(), NumWords() };
	}

	FORCEINLINE uint32& GetWord(SizeType Index)
	{
		return GetWordView()[Index];
	}
	FORCEINLINE const uint32& GetWord(SizeType Index) const
	{
		return GetWordView()[Index];
	}

private:
	using AllocatorType = typename Allocator::template ForElementType<uint32>;

	AllocatorType AllocatorInstance;
	SizeType NumBits = 0;
	SizeType MaxBits = 0;

	FORCENOINLINE void SetMaxBits(const SizeType NewMaxBits)
	{
		if (MaxBits == NewMaxBits)
		{
			return;
		}

		const SizeType PreviousMaxWords = FVoxelUtilities::DivideCeil_Positive_Log2(MaxBits, NumBitsPerWord_Log2);
		const SizeType NewMaxWords = FVoxelUtilities::DivideCeil_Positive_Log2(NewMaxBits, NumBitsPerWord_Log2);

		AllocatorInstance.ResizeAllocation(PreviousMaxWords, NewMaxWords, sizeof(uint32));

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
		const uint32 SlackMask = 0xFFFFFFFF >> (NumBitsPerWord - UsedBits);

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
		const uint32 SlackMask = 0xFFFFFFFF >> (NumBitsPerWord - UsedBits);

		ensure((GetWord(LastWordIndex) & ~SlackMask) == 0);
#endif
	}
};

template<typename Allocator>
uint32* RESTRICT GetData(TVoxelBitArray<Allocator>& Array)
{
	return Array.GetWordData();
}
template<typename Allocator>
const uint32* RESTRICT GetData(const TVoxelBitArray<Allocator>& Array)
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
using FVoxelBitArray64 = TVoxelBitArray<FDefaultAllocator64>;

template<int32 NumInlineElements>
using TVoxelInlineBitArray = TVoxelBitArray<TInlineAllocator<FMath::DivideAndRoundUp(NumInlineElements, 32)>>;

template<int32 MaxNumElements>
using TVoxelFixedBitArray = TVoxelBitArray<TFixedAllocator<FMath::DivideAndRoundUp(MaxNumElements, 32)>>;