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
	TVoxelStaticBitArray(EForceInit)
	{
		Clear();
	}

	void Clear()
	{
		Array.Memzero();
	}
	void Memzero()
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

	FORCEINLINE uint32* RESTRICT GetWordData()
	{
		return Array.GetData();
	}
	FORCEINLINE const uint32* RESTRICT GetWordData() const
	{
		return Array.GetData();
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

	FORCEINLINE void Set(const int32 Index, const bool bValue)
	{
		checkVoxelSlow(IsValidIndex(Index));

		const uint32 Mask = 1u << (Index % NumBitsPerWord);
		uint32& Value = Array[Index / NumBitsPerWord];

		if (bValue)
		{
			Value |= Mask;
		}
		else
		{
			Value &= ~Mask;
		}

		checkVoxelSlow(Test(Index) == bValue);
	}
	FORCEINLINE void SetAll(const bool bValue)
	{
		FMemory::Memset(Array.GetData(), bValue ? 0xFF : 0x00, Array.Num() * Array.GetTypeSize());
	}
	FORCEINLINE TOptional<bool> TryGetAll() const
	{
		return FVoxelBitArrayHelpers::TryGetAll(*this);
	}
	FORCEINLINE bool AllEqual(bool bValue) const
	{
		return FVoxelBitArrayHelpers::AllEqual(*this, bValue);
	}
	FORCEINLINE int32 CountSetBits() const
	{
		return FVoxelBitArrayHelpers::CountSetBits(GetWordData(), NumWords());
	}
	FORCEINLINE int32 CountSetBits(const int32 Count) const
	{
		checkVoxelSlow(Count <= Num());
		return FVoxelBitArrayHelpers::CountSetBits_UpperBound(GetWordData(), Count);
	}

	template<
		typename LambdaType,
		typename ReturnType = LambdaReturnType_T<LambdaType>,
		typename = std::enable_if_t<std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterate>>>
	FORCEINLINE EVoxelIterate ForAllSetBits(LambdaType Lambda) const
	{
		return FVoxelBitArrayHelpers::ForAllSetBits(GetWordData(), NumWords(), Num(), Lambda);
	}

	FORCEINLINE bool Test(const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return Array[Index / NumBitsPerWord] & (1u << (Index % NumBitsPerWord));
	}
	FORCEINLINE bool TestAndClear(int32 Index)
	{
		return FVoxelBitArrayHelpers::TestAndClear(*this, Index);
	}
	FORCEINLINE FVoxelBitReference operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return FVoxelBitReference(Array[Index / NumBitsPerWord], 1u << (Index % NumBitsPerWord));
	}
	FORCEINLINE FVoxelConstBitReference operator[](const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return FVoxelConstBitReference(Array[Index / NumBitsPerWord], 1u << (Index % NumBitsPerWord));
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
			return ReinterpretCastRef<TVoxelAtomic<uint32>>(Word).And_ReturnOld(Mask) & Mask;
		}
	}
	FORCEINLINE void AtomicSet(const int32 Index, const bool bValue)
	{
		AtomicSet_ReturnOld(Index, bValue);
	}

	FORCEINLINE void SetRange(const int32 Index, const int32 Num, const bool bValue)
	{
		return FVoxelBitArrayHelpers::SetRange(*this, Index, Num, bValue);
	}
	FORCEINLINE bool TestRange(int32 Index, int32 Num) const
	{
		return FVoxelBitArrayHelpers::TestRange(*this, Index, Num);
	}
	// Test a range, and clears it if all true
	FORCEINLINE bool TestAndClearRange(int32 Index, int32 Num)
	{
		return FVoxelBitArrayHelpers::TestAndClearRange(*this, Index, Num);
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelStaticBitArray& BitArray)
	{
		Ar.Serialize(BitArray.GetWordData(), BitArray.NumWords() * sizeof(uint32));
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
		return FVoxelUtilities::MemoryEqual(
			GetWordData(),
			Other.GetWordData(),
			NumWords() * sizeof(uint32));
	}

private:
	ArrayType Array{ NoInit };
};

template<uint32 Size>
constexpr bool std::is_trivially_destructible_v_voxel<TVoxelStaticBitArray<Size>> = true;