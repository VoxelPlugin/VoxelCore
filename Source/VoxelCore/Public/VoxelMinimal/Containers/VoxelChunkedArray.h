// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

// Matches FVoxelBufferStorage's chunk size for float/int32
constexpr int32 GVoxelDefaultAllocationSize = 1 << 14;

template<typename Type, int32 MaxBytesPerChunk = GVoxelDefaultAllocationSize>
class TVoxelChunkedArray
{
public:
	static constexpr int32 NumPerChunkLog2 = FVoxelUtilities::FloorLog2<FMath::DivideAndRoundDown<int32>(MaxBytesPerChunk, sizeof(TTypeCompatibleBytes<Type>))>();
	static constexpr int32 NumPerChunk = 1 << NumPerChunkLog2;

	using FChunk = TVoxelStaticArray<TTypeCompatibleBytes<Type>, NumPerChunk>;
	using FChunkArray = TVoxelInlineArray<TVoxelUniquePtr<FChunk>, 1>;

	TVoxelChunkedArray() = default;
	FORCEINLINE TVoxelChunkedArray(TVoxelChunkedArray&& Other)
		: ArrayNum(Other.ArrayNum)
		, PrivateChunks(MoveTemp(Other.PrivateChunks))
	{
		Other.ArrayNum = 0;
	}
	TVoxelChunkedArray(const TVoxelChunkedArray&) = delete;

	FORCEINLINE ~TVoxelChunkedArray()
	{
		Empty();
	}

	FORCEINLINE TVoxelChunkedArray& operator=(TVoxelChunkedArray&& Other)
	{
		ArrayNum = Other.ArrayNum;
		PrivateChunks = MoveTemp(Other.PrivateChunks);
		Other.ArrayNum = 0;
		return *this;
	}
	TVoxelChunkedArray& operator=(const TVoxelChunkedArray&) = delete;

public:
	void SetNumUninitialized(const int32 Number)
	{
		checkStatic(TIsTriviallyDestructible<Type>::Value);

		ArrayNum = Number;

		const int32 NumChunks = FVoxelUtilities::DivideCeil_Positive(ArrayNum, NumPerChunk);
		if (PrivateChunks.Num() > NumChunks)
		{
			PrivateChunks.SetNum(NumChunks);
		}
		else
		{
			VOXEL_FUNCTION_COUNTER_NUM(Number, 16384);

			PrivateChunks.Reserve(NumChunks);

			while (PrivateChunks.Num() < NumChunks)
			{
				AllocateNewChunk();
			}
		}
	}
	void Reserve(const int32 Number)
	{
		const int32 NumChunks = FVoxelUtilities::DivideCeil_Positive(Number, NumPerChunk);
		PrivateChunks.Reserve(NumChunks);
	}
	void Reset()
	{
		if (!TIsTriviallyDestructible<Type>::Value)
		{
			for (int32 Index = 0; Index < ArrayNum; Index++)
			{
				(*this)[Index].~Type();
			}
		}

		PrivateChunks.Reset();
		ArrayNum = 0;
	}
	void Empty()
	{
		Reset();
		PrivateChunks.Empty();
	}
	void Shrink()
	{
		PrivateChunks.Shrink();
	}
	void Memset(const uint8 Value)
	{
		for (int32 Index = 0; Index < PrivateChunks.Num(); Index++)
		{
			int32 NumToSet = NumPerChunk;
			if (Index == PrivateChunks.Num() - 1)
			{
				NumToSet = Num() % NumPerChunk;

				if (NumToSet == 0)
				{
					NumToSet = NumPerChunk;
				}
			}
			FVoxelUtilities::Memset(GetChunkView(Index).LeftOf(NumToSet), Value);
		}
	}

	template<typename Allocator = FVoxelAllocator>
	TVoxelArray<Type, Allocator> Array() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

		if constexpr (TIsTriviallyDestructible<Type>::Value)
		{
			TVoxelArray<Type, Allocator> Result;
			FVoxelUtilities::SetNumFast(Result, Num());

			int32 Index = 0;
			while (Index < ArrayNum)
			{
				const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
				const int32 NumInChunk = FMath::Min(NumPerChunk, ArrayNum - Index);

				FVoxelUtilities::Memcpy(
					MakeVoxelArrayView(Result).Slice(Index, NumInChunk),
					GetChunkView(ChunkIndex).LeftOf(NumInChunk));

				Index += NumInChunk;
			}
			checkVoxelSlow(Index == ArrayNum);

			return Result;
		}
		else
		{
			TVoxelArray<Type, Allocator> Result;
			Result.Reserve(Num());
			for (const Type& Value : *this)
			{
				Result.Add_NoGrow(Value);
			}
			return Result;
		}
	}

	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return PrivateChunks.GetAllocatedSize() + PrivateChunks.Num() * sizeof(FChunk);
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < ArrayNum;
	}

public:
	FORCEINLINE int32 AddUninitialized()
	{
		if (ArrayNum % NumPerChunk == 0)
		{
			AllocateNewChunk();
		}
		return ArrayNum++;
	}
	FORCEINLINE int32 AddUninitialized(const int32 Count)
	{
		checkVoxelSlow(Count >= 0);

		const int32 OldNum = ArrayNum;
		ArrayNum += Count;
		const int32 NewNum = ArrayNum;

		const int32 OldNumChunks = FVoxelUtilities::DivideCeil_Positive(OldNum, NumPerChunk);
		const int32 NewNumChunks = FVoxelUtilities::DivideCeil_Positive(NewNum, NumPerChunk);

		checkVoxelSlow(PrivateChunks.Num() == OldNumChunks);
		for (int32 ChunkIndex = OldNumChunks; ChunkIndex < NewNumChunks; ChunkIndex++)
		{
			AllocateNewChunk();
		}
		checkVoxelSlow(PrivateChunks.Num() == NewNumChunks);

		return OldNum;
	}

	FORCEINLINE int32 AddZeroed(const int32 Count)
	{
		checkStatic(TIsTriviallyDestructible<Type>::Value);
		checkVoxelSlow(Count >= 0);

		const int32 StartIndex = AddUninitialized(Count);

		const int32 EndIndex = StartIndex + Count;
		int32 Index = StartIndex;
		while (Index < EndIndex)
		{
			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
			const int32 NumInChunk = FMath::Min(NumPerChunk - ChunkOffset, EndIndex - Index);

			FVoxelUtilities::Memzero(GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk));
			Index += NumInChunk;
		}
		checkVoxelSlow(Index == EndIndex);

		return StartIndex;
	}

	FORCEINLINE int32 Add(const Type& Value)
	{
		const int32 Index = AddUninitialized();
		new (&(*this)[Index]) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		const int32 Index = AddUninitialized();
		new (&(*this)[Index]) Type(MoveTemp(Value));
		return Index;
	}
	template<typename... ArgsType>
	FORCEINLINE int32 Emplace(ArgsType&&... Args)
	{
		const int32 Index = AddUninitialized();
		new (&(*this)[Index]) Type(Forward<ArgsType>(Args)...);
		return Index;
	}

	FORCEINLINE int32 Append(const TConstVoxelArrayView<Type> Other)
	{
		const int32 StartIndex = this->AddUninitialized(Other.Num());
		if constexpr (TIsTriviallyDestructible<Type>::Value)
		{
			const int32 EndIndex = StartIndex + Other.Num();
			int32 Index = StartIndex;
			while (Index < EndIndex)
			{
				const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
				const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
				const int32 NumInChunk = FMath::Min(NumPerChunk - ChunkOffset, EndIndex - Index);

				FVoxelUtilities::Memcpy(
					GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk),
					Other.Slice(Index - StartIndex, NumInChunk));

				Index += NumInChunk;
			}
			checkVoxelSlow(Index == EndIndex);
		}
		else
		{
			for (int32 OtherIndex = 0; OtherIndex < Other.Num(); OtherIndex++)
			{
				new (&(*this)[StartIndex + OtherIndex]) Type(Other[OtherIndex]);
			}
		}
		return StartIndex;
	}

	FORCEINLINE Type& Add_GetRef(Type&& Value)
	{
		const int32 Index = AddUninitialized();
		Type& ValueRef = (*this)[Index];
		new (&ValueRef) Type(MoveTemp(Value));
		return ValueRef;
	}
	template<typename... ArgTypes>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		const int32 Index = AddUninitialized();
		Type& ValueRef = (*this)[Index];
		new (&ValueRef) Type(Forward<ArgTypes>(Args)...);
		return ValueRef;
	}

	// For compatibility with TVoxelMap
	template<typename... ArgsType>
	FORCEINLINE int32 Emplace_NoGrow(ArgsType&&... Args)
	{
		return this->Emplace(Forward<ArgsType>(Args)...);
	}

	FORCEINLINE void CopyTo(const int32 StartIndex, const Type& Value)
	{
		this->CopyTo(StartIndex, MakeVoxelArrayView(Value));
	}
	FORCEINLINE void CopyTo(const int32 StartIndex, const TConstVoxelArrayView<Type> Other)
	{
		checkVoxelSlow(IsValidIndex(StartIndex));
		checkVoxelSlow(IsValidIndex(StartIndex + Other.Num() - 1));

		if constexpr (TIsTriviallyDestructible<Type>::Value)
		{
			const int32 EndIndex = StartIndex + Other.Num();
			int32 Index = StartIndex;
			while (Index < EndIndex)
			{
				const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
				const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
				const int32 NumInChunk = FMath::Min(NumPerChunk - ChunkOffset, EndIndex - Index);

				FVoxelUtilities::Memcpy(
					GetChunkView(ChunkIndex).Slice(ChunkOffset, NumInChunk),
					Other.Slice(Index - StartIndex, NumInChunk));

				Index += NumInChunk;
			}
			checkVoxelSlow(Index == EndIndex);
		}
		else
		{
			for (int32 OtherIndex = 0; OtherIndex < Other.Num(); OtherIndex++)
			{
				(*this)[StartIndex + OtherIndex] = Type(Other[OtherIndex]);
			}
		}
	}

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return this->GetChunkView(FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index))[FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index)];
	}
	FORCEINLINE const Type& operator[](const int32 Index) const
	{
		return ConstCast(this)->operator[](Index);
	}

public:
	template<typename InType>
	struct TIterator
	{
		Type* Value = nullptr;
		const TVoxelUniquePtr<FChunk>* ChunkIterator = nullptr;
		const TVoxelUniquePtr<FChunk>* ChunkIteratorEnd = nullptr;

		FORCEINLINE InType& operator*() const
		{
			return *Value;
		}
		FORCEINLINE void operator++()
		{
			++Value;

			if (Value != ReinterpretCastPtr<Type>((*ChunkIterator)->GetData() + (*ChunkIterator)->Num()))
			{
				return;
			}

			++ChunkIterator;

			if (ChunkIterator == ChunkIteratorEnd)
			{
				// Last chunk
				Value = nullptr;
				return;
			}

			Value = ReinterpretCastPtr<Type>((*ChunkIterator)->GetData());
		}
		FORCEINLINE bool operator!=(const Type* End) const
		{
			return Value != End;
		}
	};

	FORCEINLINE TIterator<Type> begin()
	{
		if (ArrayNum == 0)
		{
			return {};
		}

		return TIterator<Type>
		{
			&GetChunkView(0)[0],
			PrivateChunks.GetData(),
			PrivateChunks.GetData() + PrivateChunks.Num()
		};
	}
	FORCEINLINE TIterator<const Type> begin() const
	{
		return ReinterpretCastRef<TIterator<const Type>>(ConstCast(this)->begin());
	}

	FORCEINLINE const Type* end() const
	{
		if (ArrayNum == 0)
		{
			return nullptr;
		}

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(ArrayNum);
		if (ChunkIndex >= PrivateChunks.Num())
		{
			// ChunkIteratorEnd will handle termination
			return nullptr;
		}

		return &GetChunkView(ChunkIndex)[FVoxelUtilities::GetChunkOffset<NumPerChunk>(ArrayNum)];
	}

private:
	int32 ArrayNum = 0;
	FChunkArray PrivateChunks;

	FORCENOINLINE void AllocateNewChunk()
	{
		PrivateChunks.Add(MakeVoxelUnique<FChunk>(NoInit));
	}
	FORCEINLINE TVoxelArrayView<Type> GetChunkView(const int32 ChunkIndex)
	{
		return ReinterpretCastVoxelArrayView<Type>(MakeVoxelArrayView(*PrivateChunks[ChunkIndex]));
	}
	FORCEINLINE TConstVoxelArrayView<Type> GetChunkView(const int32 ChunkIndex) const
	{
		return ReinterpretCastVoxelArrayView<Type>(MakeVoxelArrayView(*PrivateChunks[ChunkIndex]));
	}
};