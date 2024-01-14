// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
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
	TOptional<bool> TryGetAll() const
	{
		return FVoxelBitArrayHelpers::TryGetAll(*this);
	}
	bool AllEqual(bool bValue) const
	{
		return FVoxelBitArrayHelpers::AllEqual(*this, bValue);
	}
	int32 CountSetBits() const
	{
		return FVoxelBitArrayHelpers::CountSetBits(GetWordData(), NumWords());
	}
	int32 CountSetBits(const int32 Count) const
	{
		checkVoxelSlow(Count <= Num());
		return FVoxelBitArrayHelpers::CountSetBits_UpperBound(GetWordData(), Count);
	}

	template<typename LambdaType>
	void ForAllSetBits(LambdaType Lambda) const
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
	FORCEINLINE bool operator!=(const TVoxelStaticBitArray& Other) const
	{
		return !(*this == Other);
	}

private:
	ArrayType Array{ NoInit };
};

template<uint32 Size>
struct TIsTriviallyDestructible<TVoxelStaticBitArray<Size>>
{
	enum { Value = true };
};