// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelAtomic.h"
#include "VoxelMinimal/Containers/VoxelBitArray.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"

template<int32 InSize>
class TVoxelStaticBitArray
{
public:
	static constexpr int32 NumBitsPerWord = 32;
	static constexpr int32 Size = InSize;
	checkStatic(Size % NumBitsPerWord == 0);

	using ArrayType = TVoxelStaticArray<uint32, FVoxelUtilities::DivideCeil(Size, NumBitsPerWord)>;

	TVoxelStaticBitArray() = default;
	FORCEINLINE TVoxelStaticBitArray(EForceInit)
	{
		Clear();
	}

	FORCEINLINE void Clear()
	{
		Array.Memzero();
	}
	FORCEINLINE void Memzero()
	{
		Array.Memzero();
	}

	FORCEINLINE ArrayType& GetWordArray()
	{
		return Array;
	}
	FORCEINLINE const ArrayType& GetWordArray() const
	{
		return Array;
	}

	FORCEINLINE TVoxelArrayView<uint32> GetWordView()
	{
		return Array;
	}
	FORCEINLINE TConstVoxelArrayView<uint32> GetWordView() const
	{
		return Array;
	}

	FORCEINLINE uint32& GetWord(int32 Index)
	{
		return Array[Index];
	}
	FORCEINLINE uint32 GetWord(int32 Index) const
	{
		return Array[Index];
	}

	FORCEINLINE static constexpr int32 Num()
	{
		return Size;
	}
	FORCEINLINE static constexpr int32 NumWords()
	{
		return ArrayType::Num();
	}

	FORCEINLINE static constexpr bool IsValidIndex(const int32 Index)
	{
		return 0 <= Index && Index < Num();
	}

	FORCEINLINE void SetAll(const bool bValue)
	{
		FMemory::Memset(Array.GetData(), bValue ? 0xFF : 0x00, Array.Num() * Array.GetTypeSize());
	}
	FORCEINLINE TOptional<bool> TryGetAll() const
	{
		return FVoxelBitArrayUtilities::TryGetAll(*this);
	}
	FORCEINLINE bool AllEqual(bool bValue) const
	{
		return FVoxelBitArrayUtilities::AllEqual(*this, bValue);
	}
	FORCEINLINE int32 CountSetBits() const
	{
		return FVoxelUtilities::CountSetBits(GetWordView());
	}
	FORCEINLINE int32 CountSetBits(const int32 Count) const
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

	FORCEINLINE FVoxelBitReference operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return FVoxelBitReference(
			Array[Index >> FVoxelBitArrayUtilities::NumBitsPerWord_Log2],
			1u << (Index & FVoxelBitArrayUtilities::IndexInWordMask));
	}
	FORCEINLINE FVoxelConstBitReference operator[](const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return FVoxelConstBitReference(
			Array[Index >> FVoxelBitArrayUtilities::NumBitsPerWord_Log2],
			1u << (Index & FVoxelBitArrayUtilities::IndexInWordMask));
	}

	FORCEINLINE bool AtomicSet_ReturnOld(const int32 Index, const bool bValue)
	{
		uint32& Word = this->GetWord(Index / NumBitsPerWord);
		const uint32 Mask = 1u << (Index % NumBitsPerWord);

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

	FORCEINLINE void SetRange(const int32 Index, const int32 Num, const bool bValue)
	{
		return FVoxelBitArrayUtilities::SetRange(GetWordView(), Index, Num, bValue);
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelStaticBitArray& BitArray)
	{
		BitArray.GetWordView().Serialize(Ar);
		return Ar;
	}

	FORCEINLINE TVoxelStaticBitArray& operator&=(const TVoxelStaticBitArray& Other)
	{
		for (int32 WordIndex = 0; WordIndex < NumWords(); WordIndex++)
		{
			GetWord(WordIndex) &= Other.GetWord(WordIndex);
		}
		return *this;
	}
	FORCEINLINE TVoxelStaticBitArray& operator|=(const TVoxelStaticBitArray& Other)
	{
		for (int32 WordIndex = 0; WordIndex < NumWords(); WordIndex++)
		{
			GetWord(WordIndex) |= Other.GetWord(WordIndex);
		}
		return *this;
	}

	FORCEINLINE bool operator==(const TVoxelStaticBitArray& Other) const
	{
		return FVoxelUtilities::Equal(GetWordView(), Other.GetWordView());
	}

private:
	ArrayType Array{ NoInit };
};
checkStatic(std::is_trivially_destructible_v<TVoxelStaticBitArray<32>>);