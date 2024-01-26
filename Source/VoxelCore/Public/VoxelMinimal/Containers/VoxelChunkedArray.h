// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
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
		, Chunks(MoveTemp(Other.Chunks))
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
		Chunks = MoveTemp(Other.Chunks);
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
		if (Chunks.Num() > NumChunks)
		{
			Chunks.SetNum(NumChunks);
		}
		else
		{
			VOXEL_FUNCTION_COUNTER_NUM(Number, 16384);

			Chunks.Reserve(NumChunks);

			while (Chunks.Num() < NumChunks)
			{
				AllocateNewChunk();
			}
		}
	}
	void Reserve(const int32 Number)
	{
		const int32 NumChunks = FVoxelUtilities::DivideCeil_Positive(Number, NumPerChunk);
		Chunks.Reserve(NumChunks);
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

		Chunks.Reset();
		ArrayNum = 0;
	}
	void Empty()
	{
		Reset();
		Chunks.Empty();
	}
	void Shrink()
	{
		Chunks.Shrink();
	}
	void Memset(const uint8 Value)
	{
		for (int32 Index = 0; Index < Chunks.Num(); Index++)
		{
			int32 NumToSet = NumPerChunk;
			if (Index == Chunks.Num() - 1)
			{
				NumToSet = Num() % NumPerChunk;

				if (NumToSet == 0)
				{
					NumToSet = NumPerChunk;
				}
			}
			FVoxelUtilities::Memset(MakeVoxelArrayView(*Chunks[Index]).LeftOf(NumToSet), Value);
		}
	}
	TVoxelArray<Type> Array() const
	{
		if constexpr (TIsTriviallyDestructible<Type>::Value)
		{
			TVoxelArray<Type> Result;
			FVoxelUtilities::SetNumFast(Result, Num());

			int32 Index = 0;
			while (Index < ArrayNum)
			{
				const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
				const int32 NumInChunk = FMath::Min(NumPerChunk, ArrayNum - Index);

				FVoxelUtilities::Memcpy(
					MakeVoxelArrayView(Result).Slice(Index, NumInChunk),
					MakeVoxelArrayView(*Chunks[ChunkIndex]).LeftOf(NumInChunk));

				Index += NumInChunk;
			}
			checkVoxelSlow(Index == ArrayNum);

			return Result;
		}
		else
		{
			TVoxelArray<Type> Result;
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
		return Chunks.GetAllocatedSize() + Chunks.Num() * sizeof(FChunk);
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
		for (int32 Index = 0; Index < Count; Index++)
		{
			if (ArrayNum % NumPerChunk == 0)
			{
				AllocateNewChunk();
			}
			ArrayNum++;
		}
		return OldNum;
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
					MakeVoxelArrayView(*Chunks[ChunkIndex]).Slice(ChunkOffset, NumInChunk),
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

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return ReinterpretCastRef<Type>((*Chunks[FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index)])[FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index)]);
	}
	FORCEINLINE const Type& operator[](const int32 Index) const
	{
		return ConstCast(this)->operator[](Index);
	}

public:
	template<typename InType>
	struct TIterator
	{
		TTypeCompatibleBytes<Type>* Value = nullptr;
		const TVoxelUniquePtr<FChunk>* ChunkIterator = nullptr;
		const TVoxelUniquePtr<FChunk>* ChunkIteratorEnd = nullptr;

		FORCEINLINE InType& operator*() const
		{
			return ReinterpretCastRef<InType>(*Value);
		}
		FORCEINLINE void operator++()
		{
			++Value;

			if (Value != (*ChunkIterator)->GetData() + (*ChunkIterator)->Num())
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

			Value = (*ChunkIterator)->GetData();
		}
		FORCEINLINE bool operator!=(const TTypeCompatibleBytes<Type>* End) const
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

		return { &(*Chunks[0])[0], Chunks.GetData(), Chunks.GetData() + Chunks.Num() };
	}
	FORCEINLINE TIterator<const Type> begin() const
	{
		return ReinterpretCastRef<TIterator<const Type>>(ConstCast(this)->begin());
	}

	FORCEINLINE TTypeCompatibleBytes<Type>* end() const
	{
		if (ArrayNum == 0)
		{
			return nullptr;
		}

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(ArrayNum);
		if (ChunkIndex >= Chunks.Num())
		{
			// ChunkIteratorEnd will handle termination
			return nullptr;
		}

		return &(*Chunks[ChunkIndex])[FVoxelUtilities::GetChunkOffset<NumPerChunk>(ArrayNum)];
	}

private:
	int32 ArrayNum = 0;
	FChunkArray Chunks;

	FORCENOINLINE void AllocateNewChunk()
	{
		Chunks.Add(MakeVoxelUnique<FChunk>(NoInit));
	}
};