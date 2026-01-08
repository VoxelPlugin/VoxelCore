// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelTypeCompatibleBytes.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Containers/VoxelStaticBitArray.h"

template<typename Type, int32 NumPerChunk = 1024>
class TVoxelChunkedSparseArray
{
private:
	checkStatic(FMath::IsPowerOfTwo(NumPerChunk));

	union FValue
	{
		TVoxelTypeCompatibleBytes<Type> Value;
		int32 NextFreeIndex = -1;
	};

	struct FChunk
	{
		// Clear flags to ensure padding is always 0
		TVoxelStaticBitArray<NumPerChunk> AllocationFlags{ ForceInit };
		TVoxelStaticArray<FValue, NumPerChunk> Values{ NoInit };
	};

public:
	TVoxelChunkedSparseArray() = default;
	FORCEINLINE ~TVoxelChunkedSparseArray()
	{
		Empty();
	}

	UE_NONCOPYABLE_MOVEABLE(TVoxelChunkedSparseArray);

	void Empty()
	{
		if (!std::is_trivially_destructible_v<Type>)
		{
			VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

			for (Type& Value : *this)
			{
				Value.~Type();
			}
		}

		ArrayNum = 0;
		ArrayMax = 0;
		FirstFreeIndex = -1;
		Chunks.Empty();
	}
	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE int32 Max_Unsafe() const
	{
		return ArrayMax;
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return
			Chunks.GetAllocatedSize() +
			Chunks.Num() * sizeof(FChunk);
	}
	FORCEINLINE bool IsValidIndex_RangeOnly(const int32 Index) const
	{
		return 0 <= Index && Index < ArrayMax;
	}
	FORCEINLINE bool IsAllocated_ValidIndex(const int32 Index) const
	{
		checkVoxelSlow(IsValidIndex_RangeOnly(Index));

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);

		return this->Chunks[ChunkIndex]->AllocationFlags[ChunkOffset];
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return
			IsValidIndex_RangeOnly(Index) &&
			IsAllocated_ValidIndex(Index);
	}

public:
	FORCEINLINE int32 Add(const Type& Value)
	{
		TVoxelTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new(*ValuePtr) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		TVoxelTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new(*ValuePtr) Type(MoveTemp(Value));
		return Index;
	}

	template<typename... ArgTypes>
	requires std::is_constructible_v<Type, ArgTypes...>
	FORCEINLINE int32 Emplace(ArgTypes&&... Args)
	{
		TVoxelTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new(*ValuePtr) Type(Forward<ArgTypes>(Args)...);
		return Index;
	}
	template<typename... ArgTypes>
	requires std::is_constructible_v<Type, ArgTypes...>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		TVoxelTypeCompatibleBytes<Type>* ValuePtr;
		this->AddUninitialized(ValuePtr);
		return *new(*ValuePtr) Type(Forward<ArgTypes>(Args)...);
	}

	FORCEINLINE void RemoveAt(const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		ArrayNum--;
		checkVoxelSlow(ArrayNum >= 0);

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
		FChunk& Chunk = *this->Chunks[ChunkIndex];

		checkVoxelSlow(Chunk.AllocationFlags[ChunkOffset]);
		Chunk.AllocationFlags[ChunkOffset] = false;

		FValue& Value = Chunk.Values[ChunkOffset];

		ReinterpretCastRef<Type>(Value.Value).~Type();

		Value.NextFreeIndex = FirstFreeIndex;
		FirstFreeIndex = Index;
	}
	// Not safe if called on the same index concurrently
	// Not safe if Add is called concurrently
	FORCEINLINE void RemoveAt_Atomic(const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		ReinterpretCastRef<FVoxelCounter32>(ArrayNum).Decrement();

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
		FChunk& Chunk = *this->Chunks[ChunkIndex];

		checkVoxelSlow(Chunk.AllocationFlags[ChunkOffset]);
		const bool bWasAllocated = Chunk.AllocationFlags.AtomicSet_ReturnOld(ChunkOffset, false);
		checkVoxelSlow(bWasAllocated);

		FValue& Value = Chunk.Values[ChunkOffset];

		ReinterpretCastRef<Type>(Value.Value).~Type();

		Value.NextFreeIndex = ReinterpretCastRef<TVoxelAtomic<int32>>(FirstFreeIndex).Set_ReturnOld(Index);
	}
	FORCEINLINE Type RemoveAt_ReturnValue(const int32 Index)
	{
		Type Value = MoveTemp((*this)[Index]);
		RemoveAt(Index);
		return MoveTemp(Value);
	}

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
		return ReinterpretCastRef<Type>(this->Chunks[ChunkIndex]->Values[ChunkOffset].Value);
	}
	FORCEINLINE const Type& operator[](const int32 Index) const
	{
		return ConstCast(this)->operator[](Index);
	}

public:
	template<typename InType>
	struct TIterator : TVoxelIterator<TIterator<InType>>
	{
	public:
		using ArrayType = std::conditional_t<std::is_const_v<InType>, const TVoxelChunkedSparseArray, TVoxelChunkedSparseArray>;

		TIterator() = default;
		FORCEINLINE explicit TIterator(ArrayType& Array)
			: Array(Array)
		{
			if (Array.Num() == 0)
			{
				// No need to iterate the full array
				return;
			}

			Chunks = Array.Chunks;
			LoadChunk();
		}

		FORCEINLINE int32 GetIndex() const
		{
			return ChunkIndex * NumPerChunk + Iterator.GetIndex();
		}
		FORCEINLINE void RemoveCurrent() const
		{
			Array.RemoveAt(GetIndex());
		}

		FORCEINLINE InType& operator*() const
		{
			InType& Result = ReinterpretCastRef<InType>(Values[Iterator.GetIndex()]);
			checkVoxelSlow(&Result == &Array[GetIndex()]);
			return Result;
		}
		FORCEINLINE void operator++()
		{
			++Iterator;

			if (!Iterator)
			{
				ChunkIndex++;
				LoadChunk();
			}
		}
		FORCEINLINE operator bool() const
		{
			return ChunkIndex < Chunks.Num();
		}

	private:
		ArrayType& Array;

		TConstVoxelArrayView<TUniquePtr<FChunk>> Chunks;
		int32 ChunkIndex = 0;

		TVoxelArrayView<FValue> Values;
		FVoxelSetBitIterator Iterator;

		void LoadChunk()
		{
			while (ChunkIndex != Chunks.Num())
			{
				FChunk& Chunk = *Chunks[ChunkIndex];

				Values = Chunk.Values;
				Iterator = FVoxelSetBitIterator(Chunk.AllocationFlags.View());

				if (Iterator)
				{
					return;
				}

				ChunkIndex++;
			}
		}
	};
	using FIterator = TIterator<Type>;
	using FConstIterator = TIterator<const Type>;

	FORCEINLINE FIterator CreateIterator()
	{
		return FIterator(*this);
	}
	FORCEINLINE FConstIterator CreateIterator() const
	{
		return FConstIterator(*this);
	}

	FORCEINLINE FIterator begin()
	{
		return FIterator(*this);
	}
	FORCEINLINE FConstIterator begin() const
	{
		return FConstIterator(*this);
	}
	FORCEINLINE FVoxelIteratorEnd end() const
	{
		return FIterator::end();
	}

private:
	int32 ArrayNum = 0;
	int32 ArrayMax = 0;
	int32 FirstFreeIndex = -1;
	TVoxelInlineArray<TUniquePtr<FChunk>, 1> Chunks;

	FORCEINLINE int32 AddUninitialized(TVoxelTypeCompatibleBytes<Type>*& OutValuePtr)
	{
		ArrayNum++;

		if (FirstFreeIndex != -1)
		{
			const int32 Index = FirstFreeIndex;

			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
			FChunk& Chunk = *Chunks[ChunkIndex];
			FValue& Value = Chunk.Values[ChunkOffset];

			FirstFreeIndex = Value.NextFreeIndex;

			checkVoxelSlow(!Chunk.AllocationFlags[ChunkOffset]);
			Chunk.AllocationFlags[ChunkOffset] = true;

			OutValuePtr = &Value.Value;
			return Index;
		}
		else
		{
			if (ArrayMax % NumPerChunk == 0)
			{
				AllocateNewChunk();
			}
			const int32 Index = ArrayMax++;

			const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
			const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);
			FChunk& Chunk = *Chunks[ChunkIndex];

			checkVoxelSlow(!Chunk.AllocationFlags[ChunkOffset]);
			Chunk.AllocationFlags[ChunkOffset] = true;

			OutValuePtr = &Chunk.Values[ChunkOffset].Value;
			return Index;
		}
	}
	FORCENOINLINE void AllocateNewChunk()
	{
		Chunks.Add(MakeUnique<FChunk>());
	}
};