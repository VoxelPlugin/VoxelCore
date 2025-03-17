// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelBufferPoolBase;
class FVoxelBufferPool;
class FVoxelTextureBufferPool;

class VOXELCORE_API FVoxelBufferRef
{
public:
	FVoxelBufferRef(
		FVoxelBufferPoolBase& Pool,
		int32 PoolIndex,
		int64 Index,
		int64 Num);
	~FVoxelBufferRef();
	UE_NONCOPYABLE(FVoxelBufferRef);

	VOXEL_COUNT_INSTANCES();

	FORCEINLINE bool IsOutOfMemory() const
	{
		return PoolIndex == -1;
	}
	FORCEINLINE int64 Num() const
	{
		return PrivateNum;
	}
	FORCEINLINE int64 GetIndex() const
	{
		return Index;
	}

private:
	const TWeakPtr<FVoxelBufferPoolBase> WeakPool;
	const int32 PoolIndex;
	const int64 Index;
	const int64 PrivateNum;

	friend FVoxelBufferPoolBase;
	friend FVoxelBufferPool;
	friend FVoxelTextureBufferPool;
};

class VOXELCORE_API FVoxelBufferPoolBase : public TSharedFromThis<FVoxelBufferPoolBase>
{
public:
	const int32 BytesPerElement;
	const EPixelFormat PixelFormat;
	const FString BufferName;
	FTSSimpleMulticastDelegate OnOutOfMemory;

	FVoxelBufferPoolBase(
		int32 BytesPerElement,
		EPixelFormat PixelFormat,
		const FString& BufferName);
	virtual ~FVoxelBufferPoolBase();

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

protected:
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
		const FSharedVoidPtr& Owner,
		TConstVoxelArrayView64<uint8> Data,
		const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef = nullptr);

	TVoxelFuture<FVoxelBufferRef> Upload_AnyThread(
		TVoxelArray<uint8> Data,
		const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef = nullptr);

	template<typename T>
	TVoxelFuture<FVoxelBufferRef> Upload_AnyThread(
		TVoxelArray<T> Data,
		const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef = nullptr)
	{
		check(sizeof(T) == BytesPerElement);

		const TSharedRef<TVoxelArray<T>> SharedData = MakeSharedCopy(MoveTemp(Data));

		return this->Upload_AnyThread(
			MakeSharedVoidRef(SharedData),
			MakeByteVoxelArrayView(*SharedData),
			ExistingBufferRef);
	}

protected:
	struct FAllocationPool
	{
	public:
		const int64 PoolSize;

		explicit FAllocationPool(const int32 PoolIndex)
			: PoolSize(GetPoolSize(PoolIndex))
		{
		}

		int64 Allocate(FVoxelBufferPoolBase& Pool);
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
		checkVoxelSlow(PoolIndex == 0 || GetPoolSize(PoolIndex - 1) < Num);
		checkVoxelSlow(Num <= GetPoolSize(PoolIndex));

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

	FVoxelCriticalSection BufferCount_CriticalSection;
	FVoxelCounter64 BufferCount;

	FVoxelCriticalSection PoolIndexToPool_CriticalSection;
	TVoxelArray<FAllocationPool> PoolIndexToPool_RequiresLock;

	friend FVoxelBufferRef;

protected:
	struct FUpload
	{
		FSharedVoidPtr Owner;
		TConstVoxelArrayView64<uint8> Data;
		TSharedPtr<FVoxelBufferRef> BufferRef;
		TSharedPtr<TVoxelPromise<FVoxelBufferRef>> BufferRefPromise;

		FORCEINLINE int64 NumBytes() const
		{
			return Data.Num();
		}
	};

	TVoxelAtomic<bool> IsProcessingUploads = false;

	TQueue<FUpload, EQueueMode::Mpsc> UploadQueue;

	void CheckUploadQueue_AnyThread();
	FVoxelFuture ProcessUploads_AnyThread();

	virtual int64 GetMaxAllocatedNum() const = 0;
	virtual FVoxelFuture ProcessUploadsImpl_AnyThread(TVoxelArray<FUpload>&& Uploads) = 0;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelBufferPool : public FVoxelBufferPoolBase
{
public:
	using FVoxelBufferPoolBase::FVoxelBufferPoolBase;
	virtual ~FVoxelBufferPool() override = default;

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

protected:
	//~ Begin FVoxelBufferPoolBase Interface
	virtual int64 GetMaxAllocatedNum() const override;
	virtual FVoxelFuture ProcessUploadsImpl_AnyThread(TVoxelArray<FUpload>&& Uploads) override;
	//~ End FVoxelBufferPoolBase Interface

private:
	FBufferRHIRef BufferRHI_RenderThread;
	FShaderResourceViewRHIRef BufferSRV_RenderThread;

private:
	struct FCopyInfo
	{
		TSharedPtr<FVoxelBufferRef> BufferRef;
		TSharedPtr<TVoxelPromise<FVoxelBufferRef>> BufferRefPromise;
		FBufferRHIRef SourceBuffer;
		int64 SourceOffset = 0;
	};

	void ProcessCopies_RenderThread(
		FRHICommandList& RHICmdList,
		TConstVoxelArrayView<FCopyInfo> CopyInfos);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelTextureBufferPool : public FVoxelBufferPoolBase
{
public:
	const int32 MaxTextureSize;

	FVoxelTextureBufferPool(
		int32 BytesPerElement,
		EPixelFormat PixelFormat,
		const FString& BufferName,
		int32 MaxTextureSize);

	virtual ~FVoxelTextureBufferPool() override;

public:
	FORCEINLINE UTexture2D* GetTexture_GameThread() const
	{
		checkVoxelSlow(IsInGameThread());
		return Texture_GameThread;
	}
	FORCEINLINE FTextureRHIRef GetTextureRHI_RenderThread() const
	{
		checkVoxelSlow(IsInParallelRenderingThread());
		return TextureRHI_RenderThread;
	}
	FORCEINLINE int32 GetTextureSizeLog2_RenderThread() const
	{
		checkVoxelSlow(IsInParallelRenderingThread());

		if (!TextureRHI_RenderThread)
		{
			return 0;
		}

		return FVoxelUtilities::ExactLog2(TextureRHI_RenderThread->GetSizeX());
	}

protected:
	//~ Begin FVoxelBufferPoolBase Interface
	virtual int64 GetMaxAllocatedNum() const override;
	virtual FVoxelFuture ProcessUploadsImpl_AnyThread(TVoxelArray<FUpload>&& Uploads) override;
	//~ End FVoxelBufferPoolBase Interface

private:
	TObjectPtr<UTexture2D> Texture_GameThread;
	FTextureRHIRef TextureRHI_RenderThread;

	void AddReferencedObjects(FReferenceCollector& Collector);

	friend class FVoxelTextureBufferPoolSingleton;
};