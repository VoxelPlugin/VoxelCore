// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"

template<typename>
struct TVoxelLinkedArrayHandle
{
public:
	TVoxelLinkedArrayHandle() = default;

	FORCEINLINE bool IsValid() const
	{
		return ChunkIndex != -1;
	}

private:
	int32 ChunkIndex = -1;

	template<typename, int32>
	friend class TVoxelLinkedArray;
};

template<typename Type, int32 NumPerChunk = 14>
class TVoxelLinkedArray;

template<typename Type, int32 NumPerChunk>
requires std::is_trivially_destructible_v<Type>
class TVoxelLinkedArray<Type, NumPerChunk>
{
public:
	TVoxelLinkedArray() = default;

	FORCEINLINE TVoxelLinkedArrayHandle<Type> Allocate()
	{
		TVoxelLinkedArrayHandle<Type> Result;
		Result.ChunkIndex = Chunks.Emplace();
		return Result;
	}
	FORCEINLINE void AddTo(
		TVoxelLinkedArrayHandle<Type>& Handle,
		const Type& Value)
	{
		checkVoxelSlow(Handle.IsValid());

		FChunk& Chunk = Chunks[Handle.ChunkIndex];
		if (Chunk.Num < NumPerChunk)
		{
			Chunk.Values[Chunk.Num++] = Value;
			return;
		}

		const int32 NewChunkIndex = Chunks.Emplace();

		FChunk& NewChunk = Chunks[NewChunkIndex];
		NewChunk.NextChunkIndex = Handle.ChunkIndex;
		NewChunk.Values[NewChunk.Num++] = Value;

		Handle.ChunkIndex = NewChunkIndex;
	}

private:
	struct FChunk
	{
		TVoxelStaticArray<Type, NumPerChunk> Values{ NoInit };
		int32 Num = 0;
		int32 NextChunkIndex = -1;

		FORCEINLINE bool TryAdd(const int32 Index)
		{
			if (Num == Values.Num())
			{
				return false;
			}

			Values[Num++] = Index;
			return true;
		}
	};
	TVoxelChunkedArray<FChunk> Chunks;

public:
	struct FIterator : TVoxelRangeIterator<FIterator>
	{
	public:
		FORCEINLINE const Type& operator*() const
		{
			checkVoxelSlow(IndexInChunk < Chunk->Num);
			return Chunk->Values[IndexInChunk];
		}
		FORCEINLINE void operator++()
		{
			IndexInChunk++;

			if (IndexInChunk < NumPerChunk)
			{
				if (IndexInChunk == Chunk->Num)
				{
					Chunk = nullptr;
				}
				return;
			}
			checkVoxelSlow(IndexInChunk == NumPerChunk);

			if (Chunk->NextChunkIndex == -1)
			{
				Chunk = nullptr;
				return;
			}

			Chunk = &Array.Chunks[Chunk->NextChunkIndex];
			IndexInChunk = 0;
		}
		FORCEINLINE operator bool() const
		{
			return Chunk != nullptr;
		}

	private:
		const TVoxelLinkedArray& Array;
		const FChunk* Chunk = nullptr;
		int32 IndexInChunk = 0;

		FORCEINLINE explicit FIterator(
			const TVoxelLinkedArray& Array,
			const int32 ChunkIndex)
			: Array(Array)
		{
			Chunk = &Array.Chunks[ChunkIndex];
		}

		friend TVoxelLinkedArray;
	};

	FORCEINLINE FIterator Iterate(const TVoxelLinkedArrayHandle<Type>& Handle) const
	{
		checkVoxelSlow(Handle.IsValid());

		return FIterator(*this, Handle.ChunkIndex);
	}
};