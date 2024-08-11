// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelBufferPool;

class VOXELCORE_API FVoxelBufferRef
{
public:
	FVoxelBufferRef(
		FVoxelBufferPool& Pool,
		int32 PoolIndex,
		int64 Index,
		int64 Num);
	~FVoxelBufferRef();
	UE_NONCOPYABLE(FVoxelBufferRef);

	VOXEL_COUNT_INSTANCES();

	FORCEINLINE int64 Num() const
	{
		return PrivateNum;
	}
	FORCEINLINE int64 GetIndex() const
	{
		return Index;
	}

private:
	const TWeakPtr<FVoxelBufferPool> WeakPool;
	const int32 PoolIndex;
	const int64 Index;
	const int64 PrivateNum;

	friend FVoxelBufferPool;
};

class VOXELCORE_API FVoxelBufferPool : public TSharedFromThis<FVoxelBufferPool>
{
public:
	const int32 BytesPerElement;
	const EPixelFormat PixelFormat;

	FVoxelBufferPool(
		int32 BytesPerElement,
		EPixelFormat PixelFormat,
		const TCHAR* BufferName);
	~FVoxelBufferPool();

public:
	FORCEINLINE FRHIBuffer* GetRHI_RenderThread() const
	{
		checkVoxelSlow(IsInParallelRenderingThread());
		return BufferRHI_RenderThread;
	}
	FORCEINLINE FRHIShaderResourceView* GetSRV_RenderThread() const
	{
		checkVoxelSlow(IsInParallelRenderingThread());
		return BufferSRV_RenderThread;
	}

private:
	const TCHAR* const BufferName;

	FBufferRHIRef BufferRHI_RenderThread;
	FShaderResourceViewRHIRef BufferSRV_RenderThread;

public:
	FORCEINLINE int64 GetAllocatedMemory() const
	{
		return AllocatedMemory.Get();
	}
	FORCEINLINE int64 GetUsedMemory() const
	{
		return UsedMemory.Get();
	}
	FORCEINLINE int64 GetPaddingMemory() const
	{
		return PaddingMemory.Get();
	}

private:
	const FName AllocatedMemory_Name;
	const FName UsedMemory_Name;
	const FName PaddingMemory_Name;

	FVoxelCounter64 AllocatedMemory;
	FVoxelCounter64 UsedMemory;
	FVoxelCounter64 PaddingMemory;

	FVoxelCounter64 AllocatedMemory_Reported;
	FVoxelCounter64 UsedMemory_Reported;
	FVoxelCounter64 PaddingMemory_Reported;

	void UpdateStats();

public:
	TSharedRef<FVoxelBufferRef> Allocate_AnyThread(int64 Num);

	TVoxelFuture<FVoxelBufferRef> Upload_AnyThread(
		const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef,
		const FSharedVoidPtr& Owner,
		TConstVoxelArrayView64<uint8> Data);

private:
	struct FAllocationPool
	{
	public:
		const int64 PoolSize;

		explicit FAllocationPool(const int32 PoolIndex)
			: PoolSize(GetPoolSize(PoolIndex))
		{
		}

		int64 Allocate(FVoxelBufferPool& Pool);
		void Free(int64 Index);

	private:
		FVoxelCriticalSection_NoPadding CriticalSection;
		TVoxelArray<int64> FreeIndices_RequiresLock;
	};

	FORCEINLINE static int32 NumToPoolIndex(const int64 Num)
	{
		const int32 PoolIndex = INLINE_LAMBDA -> int32
		{
			// 0 <= PoolIndex <= 10
			if (Num <= 1024)
			{
				return FMath::CeilLogTwo(Num);
			}

			if (Num <= 64 * 1024)
			{
				// 11 <= PoolIndex <= 73
				return 10 + FVoxelUtilities::DivideCeil_Positive<int64>(Num, 1024) - 1;
			}

			// 74 <= PoolIndex
			// Some pools will be empty but it makes the math easier
			return 74 + FMath::CeilLogTwo(Num);
		};
		checkVoxelSlow(GetPoolSize(PoolIndex - 1) < Num && Num <= GetPoolSize(PoolIndex));

		return PoolIndex;
	}
	FORCEINLINE static int64 GetPoolSize(const int32 PoolIndex)
	{
		checkVoxelSlow(PoolIndex >= 0);

		if (PoolIndex <= 10)
		{
			return int64(1) << PoolIndex;
		}

		if (PoolIndex <= 73)
		{
			return (PoolIndex - 10 + 1) * 1024;
		}

		return int64(1) << (PoolIndex - 74);
	}

	FVoxelCounter64 BufferCount;

	FVoxelCriticalSection PoolIndexToPool_CriticalSection;
	TVoxelArray<FAllocationPool> PoolIndexToPool_RequiresLock;

	friend FVoxelBufferRef;

private:
	TVoxelAtomic<bool> IsProcessingUploads = false;

	struct FUpload
	{
		FSharedVoidPtr Owner;
		TConstVoxelArrayView64<uint8> Data;
		TSharedPtr<FVoxelBufferRef> ExistingBufferRef;
		TVoxelPromise<FVoxelBufferRef> BufferRefPromise;

		FORCEINLINE int64 NumBytes() const
		{
			return Data.Num();
		}
	};
	TQueue<FUpload, EQueueMode::Mpsc> UploadQueue;

	struct FCopyInfo
	{
		TSharedPtr<FVoxelBufferRef> BufferRef;
		TVoxelPromise<FVoxelBufferRef> BufferRefPromise;
		FBufferRHIRef SourceBuffer;
		int64 SourceOffset = 0;
	};

	void CheckUploadQueue_AnyThread();

	TVoxelArray<FCopyInfo> ProcessUploads_AnyThread();

	void ProcessCopies_RenderThread(
		FRHICommandList& RHICmdList,
		TConstVoxelArrayView<FCopyInfo> CopyInfos);
};