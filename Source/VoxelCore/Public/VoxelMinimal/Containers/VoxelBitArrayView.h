// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelBitArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

template<typename SizeType>
class TConstVoxelBitArrayView
{
public:
	static constexpr int32 NumBitsPerWord = FVoxelBitArrayUtilities::NumBitsPerWord;
	static constexpr int32 NumBitsPerWord_Log2 = FVoxelBitArrayUtilities::NumBitsPerWord_Log2;
	static constexpr uint32 IndexInWordMask = FVoxelBitArrayUtilities::IndexInWordMask;

	TConstVoxelBitArrayView() = default;

	template<typename Allocator>
	requires std::is_same_v<typename Allocator::SizeType, SizeType>
	FORCEINLINE TConstVoxelBitArrayView(const TVoxelBitArray<Allocator>& BitArray)
	{
		Allocation = ConstCast(BitArray.GetWordData());
		NumBits = BitArray.NumBits;
	}

public:
	template<typename OtherSizeType>
	FORCEINLINE bool operator==(const TConstVoxelBitArrayView<OtherSizeType>& Other) const
	{
		if (Num() != Other.Num())
		{
			return false;
		}

		return FVoxelUtilities::Equal(GetWordView(), Other.GetWordView());
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

	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(int32)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(int32)>
	)
	FORCEINLINE EVoxelIterate ForAllSetBits(LambdaType Lambda) const
	{
		return FVoxelBitArrayUtilities::ForAllSetBits(GetWordView(), Num(), Lambda);
	}

public:
	FORCEINLINE SizeType Num() const
	{
		return NumBits;
	}
	FORCEINLINE SizeType NumWords() const
	{
		return FVoxelUtilities::DivideCeil_Positive_Log2(NumBits, NumBitsPerWord_Log2);
	}

public:
	FORCEINLINE bool IsValidIndex(SizeType Index) const
	{
		return 0 <= Index && Index < NumBits;
	}

	FORCEINLINE FVoxelConstBitReference operator[](SizeType Index) const
	{
		checkVoxelSlow(this->IsValidIndex(Index));
		return FVoxelConstBitReference(
			this->GetWord(Index >> NumBitsPerWord_Log2),
			1u << (Index & IndexInWordMask));
	}

public:
	FORCEINLINE const uint32* GetWordData() const
	{
		return Allocation;
	}
	FORCEINLINE TConstVoxelArrayView<uint32, SizeType> GetWordView() const
	{
		return { GetWordData(), NumWords() };
	}
	FORCEINLINE const uint32& GetWord(SizeType Index) const
	{
		return GetWordView()[Index];
	}

protected:
	uint32* RESTRICT Allocation = nullptr;
	SizeType NumBits = 0;

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename SizeType>
class TVoxelBitArrayView : public TConstVoxelBitArrayView<SizeType>
{
public:
	static constexpr int32 NumBitsPerWord = FVoxelBitArrayUtilities::NumBitsPerWord;
	static constexpr int32 NumBitsPerWord_Log2 = FVoxelBitArrayUtilities::NumBitsPerWord_Log2;
	static constexpr uint32 IndexInWordMask = FVoxelBitArrayUtilities::IndexInWordMask;

	TVoxelBitArrayView() = default;

	template<typename Allocator>
	requires std::is_same_v<typename Allocator::SizeType, SizeType>
	FORCEINLINE TVoxelBitArrayView(TVoxelBitArray<Allocator>& BitArray)
		: TConstVoxelBitArrayView<SizeType>(BitArray)
	{
	}

public:
	void BitwiseOr(const TVoxelBitArrayView& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(this->Num(), 128);
		checkVoxelSlow(this->Num() == Other.Num());

		this->EnsurePartialSlackBitsCleared();
		Other.EnsurePartialSlackBitsCleared();

		for (int32 WordIndex = 0; WordIndex < this->NumWords(); WordIndex++)
		{
			GetWord(WordIndex) |= Other.GetWord(WordIndex);
		}
	}
	void BitwiseAnd(const TVoxelBitArrayView& Other) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(this->Num(), 128);
		checkVoxelSlow(this->Num() == Other.Num());

		this->EnsurePartialSlackBitsCleared();
		Other.EnsurePartialSlackBitsCleared();

		for (int32 WordIndex = 0; WordIndex < this->NumWords(); WordIndex++)
		{
			GetWord(WordIndex) &= Other.GetWord(WordIndex);
		}
	}

public:
	FORCEINLINE void SetRange(SizeType Index, SizeType Num, bool Value) const
	{
		FVoxelBitArrayUtilities::SetRange(GetWordView(), Index, Num, Value);
	}

public:
	FORCEINLINE FVoxelBitReference operator[](SizeType Index) const
	{
		checkVoxelSlow(this->IsValidIndex(Index));
		return FVoxelBitReference(
			this->GetWord(Index >> NumBitsPerWord_Log2),
			1u << (Index & IndexInWordMask));
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue) const
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
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue) const
	{
		(void)AtomicSet_ReturnOld(Index, bValue);
	}

public:
	FORCEINLINE uint32* GetWordData() const
	{
		return this->Allocation;
	}
	FORCEINLINE TVoxelArrayView<uint32, SizeType> GetWordView() const
	{
		return { GetWordData(), this->NumWords() };
	}
	FORCEINLINE uint32& GetWord(SizeType Index) const
	{
		return GetWordView()[Index];
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

using FVoxelBitArrayView = TVoxelBitArrayView<int32>;
using FVoxelBitArrayView64 = TVoxelBitArrayView<int64>;

using FConstVoxelBitArrayView = TConstVoxelBitArrayView<int32>;
using FConstVoxelBitArrayView64 = TConstVoxelBitArrayView<int64>;