// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelTypeCompatibleBytes.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Containers/VoxelStaticBitArray.h"
#include "VoxelMinimal/Utilities/VoxelThreadingUtilities.h"

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

			this->Foreach([&](Type& Value)
			{
				Value.~Type();
			});
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
	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(Type)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(const Type&)>
	)
	FORCEINLINE EVoxelIterate Foreach(LambdaType Lambda)
	{
		for (const TUniquePtr<FChunk>& Chunk : Chunks)
		{
			const EVoxelIterate Iterate = Chunk->AllocationFlags.ForAllSetBits([&](const int32 Index)
			{
				return Lambda(ReinterpretCastRef<Type>(Chunk->Values[Index].Value));
			});

			if (Iterate == EVoxelIterate::Stop)
			{
				return EVoxelIterate::Stop;
			}
		}

		return EVoxelIterate::Continue;
	}
	template<typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(Type)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(const Type&)>
	)
	FORCEINLINE EVoxelIterate Foreach(LambdaType Lambda) const
	{
		return ConstCast(this)->Foreach(MoveTemp(Lambda));
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
	struct TIterator
	{
	public:
		FORCEINLINE InType& operator*() const
		{
#if VOXEL_DEBUG
			int32 DebugIndex = ValuePtr - (**ChunkPtr).Values.GetData();
			check((**ChunkPtr).Values.IsValidIndex(DebugIndex));

			const int32 ChunkIndex = ChunkPtr - Array->Chunks.GetData();
			check(Array->Chunks.IsValidIndex(ChunkIndex));
			DebugIndex += ChunkIndex * NumPerChunk;
#endif
			return ReinterpretCastRef<InType>(*ValuePtr);
		}
		FORCEINLINE void operator++()
		{
#if VOXEL_DEBUG
			int32 DebugIndex;
			{
				DebugIndex = ValuePtr - (**ChunkPtr).Values.GetData();
				check((**ChunkPtr).Values.IsValidIndex(DebugIndex));

				const int32 ChunkIndex = ChunkPtr - Array->Chunks.GetData();
				check(Array->Chunks.IsValidIndex(ChunkIndex));
				DebugIndex += ChunkIndex * NumPerChunk;
			}

			check(Array->IsAllocated_ValidIndex(DebugIndex));

			ON_SCOPE_EXIT
			{
				if (ChunkPtr == LastChunkPtr)
				{
					for (int32 Index = DebugIndex + 1; Index < Array->Max_Unsafe(); Index++)
					{
						check(!Array->IsAllocated_ValidIndex(Index));
					}
					return;
				}

				int32 NewDebugIndex = ValuePtr - (**ChunkPtr).Values.GetData();
				check((**ChunkPtr).Values.IsValidIndex(NewDebugIndex));

				const int32 ChunkIndex = ChunkPtr - Array->Chunks.GetData();
				check(Array->Chunks.IsValidIndex(ChunkIndex));
				NewDebugIndex += ChunkIndex * NumPerChunk;

				for (int32 Index = DebugIndex + 1; Index < NewDebugIndex; Index++)
				{
					check(!Array->IsAllocated_ValidIndex(Index));
				}
				check(Array->IsAllocated_ValidIndex(NewDebugIndex));
			};
#endif

			ValuePtr++;
			Word >>= 1;
			BitsLeftInWord--;

			FindFirstSetBit();
		}
		FORCEINLINE bool operator!=(const int32) const
		{
			checkVoxelSlow(ChunkPtr <= LastChunkPtr);
			return ChunkPtr != LastChunkPtr;
		}

	private:
		TUniquePtr<FChunk>* ChunkPtr = nullptr;
		TUniquePtr<FChunk>* LastChunkPtr = nullptr;
		int32 ArrayMax = 0;

		uint32* WordPtr = nullptr;
		uint32* LastWordPtr = nullptr;
		FValue* ValuePtr = nullptr;
		uint32 Word = 0;
		int32 BitsLeftInWord = 0;

#if VOXEL_DEBUG
		TVoxelChunkedSparseArray* Array = nullptr;
#endif

		TIterator() = default;

		FORCEINLINE explicit TIterator(TVoxelChunkedSparseArray& Array)
			: ChunkPtr(Array.Chunks.GetData())
			, LastChunkPtr(ChunkPtr + Array.Chunks.Num())
			, ArrayMax(Array.ArrayMax)
#if VOXEL_DEBUG
			, Array(&Array)
#endif
		{
			LoadChunk();
			FindFirstSetBit();
		}

		FORCEINLINE void LoadChunk()
		{
			checkVoxelSlow(ChunkPtr);
			FChunk& Chunk = **ChunkPtr;

			WordPtr = Chunk.AllocationFlags.GetWordView().GetData();
			LastWordPtr = WordPtr + Chunk.AllocationFlags.NumWords();
			ValuePtr = Chunk.Values.GetData();

			if (ChunkPtr == LastChunkPtr - 1 &&
				(ArrayMax % NumPerChunk) != 0)
			{
				// Last chunk, don't iterate all the empty words at the end of the chunk
				LastWordPtr = WordPtr + FVoxelUtilities::DivideCeil_Positive(ArrayMax % NumPerChunk, 32);
			}

			checkVoxelSlow(WordPtr);

			Word = *WordPtr;
			BitsLeftInWord = 32;
		}

		FORCEINLINE void FindFirstSetBit()
		{
		NextBit:
			checkVoxelSlow(BitsLeftInWord >= 0);
			checkVoxelSlow(BitsLeftInWord > 0 || !Word);

			if (Word & 0x1)
			{
				// Fast path, no need to skip
				return;
			}

			if (Word)
			{
				// There's still a valid index, skip to that

				const uint32 IndexInShiftedWord = FVoxelUtilities::FirstBitLow(Word);
				checkVoxelSlow(Word & (1u << IndexInShiftedWord));
				// If 0 handled by fast path above
				checkVoxelSlow(IndexInShiftedWord > 0);

				ValuePtr += IndexInShiftedWord;
				Word >>= IndexInShiftedWord;
				BitsLeftInWord -= IndexInShiftedWord;

				checkVoxelSlow(Word & 0x1);
				checkVoxelSlow(BitsLeftInWord > 0);
				return;
			}

			// Skip forward
			WordPtr++;
			ValuePtr += BitsLeftInWord;

			if (WordPtr == LastWordPtr)
			{
				goto LoadNextChunk;
			}
			checkVoxelSlow(WordPtr < LastWordPtr);

			Word = *WordPtr;
			BitsLeftInWord = 32;

			while (!Word)
			{
				// Skip forward
				WordPtr++;
				ValuePtr += 32;

				if (WordPtr == LastWordPtr)
				{
					goto LoadNextChunk;
				}
				checkVoxelSlow(WordPtr < LastWordPtr);

				Word = *WordPtr;
			}

			goto NextBit;

		LoadNextChunk:
			ChunkPtr++;

			if (ChunkPtr == LastChunkPtr)
			{
				return;
			}

			LoadChunk();
			goto NextBit;
		}

		friend TVoxelChunkedSparseArray;
	};

	FORCEINLINE TIterator<Type> begin()
	{
		if (ArrayNum == 0)
		{
			// No need to iterate the full array
			return {};
		}

		return TIterator<Type>(*this);
	}
	FORCEINLINE TIterator<const Type> begin() const
	{
		return ReinterpretCastRef<TIterator<const Type>>(ConstCast(this)->begin());
	}
	FORCEINLINE int32 end() const
	{
		return 0;
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