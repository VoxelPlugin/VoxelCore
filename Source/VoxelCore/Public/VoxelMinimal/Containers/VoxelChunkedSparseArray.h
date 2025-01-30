// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"
#include "VoxelMinimal/Containers/VoxelStaticBitArray.h"

template<typename Type, int32 NumPerChunk = 1024>
class TVoxelChunkedSparseArray
{
public:
	TVoxelChunkedSparseArray() = default;
	FORCEINLINE ~TVoxelChunkedSparseArray()
	{
		Empty();
	}

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
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return
			Chunks.GetAllocatedSize() +
			Chunks.Num() * sizeof(FChunk);
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		if (!(0 <= Index && Index < ArrayMax))
		{
			return false;
		}

		const int32 ChunkIndex = FVoxelUtilities::GetChunkIndex<NumPerChunk>(Index);
		const int32 ChunkOffset = FVoxelUtilities::GetChunkOffset<NumPerChunk>(Index);

		return this->Chunks[ChunkIndex]->AllocationFlags[ChunkOffset];
	}

	template<typename LambdaType, typename = std::enable_if_t<
		LambdaHasSignature_V<LambdaType, void(Type)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(const Type&)>>>
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
	template<typename LambdaType, typename = std::enable_if_t<
		LambdaHasSignature_V<LambdaType, void(Type)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(Type)> ||
		LambdaHasSignature_V<LambdaType, EVoxelIterate(const Type&)>>>
	FORCEINLINE EVoxelIterate Foreach(LambdaType Lambda) const
	{
		return ConstCast(this)->Foreach(MoveTemp(Lambda));
	}

public:
	FORCEINLINE int32 Add(const Type& Value)
	{
		TTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new (ValuePtr) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		TTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new (ValuePtr) Type(MoveTemp(Value));
		return Index;
	}

	template<typename... ArgTypes, typename = std::enable_if_t<TIsConstructible<Type, ArgTypes...>::Value>>
	FORCEINLINE int32 Emplace(ArgTypes&&... Args)
	{
		TTypeCompatibleBytes<Type>* ValuePtr;
		const int32 Index = this->AddUninitialized(ValuePtr);
		new (ValuePtr) Type(Forward<ArgTypes>(Args)...);
		return Index;
	}
	template<typename... ArgTypes, typename = std::enable_if_t<TIsConstructible<Type, ArgTypes...>::Value>>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		TTypeCompatibleBytes<Type>* ValuePtr;
		this->AddUninitialized(ValuePtr);
		return *new (ValuePtr) Type(Forward<ArgTypes>(Args)...);
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

private:
	union FValue
	{
		TTypeCompatibleBytes<Type> Value;
		int32 NextFreeIndex = -1;
	};
	int32 ArrayNum = 0;
	int32 ArrayMax = 0;
	int32 FirstFreeIndex = -1;

	struct FChunk
	{
		// Clear flags to ensure padding is always 0
		TVoxelStaticBitArray<NumPerChunk> AllocationFlags{ ForceInit };
		TVoxelStaticArray<FValue, NumPerChunk> Values{ NoInit };
	};
	TVoxelInlineArray<TUniquePtr<FChunk>, 1> Chunks;

	FORCEINLINE int32 AddUninitialized(TTypeCompatibleBytes<Type>*& OutValuePtr)
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