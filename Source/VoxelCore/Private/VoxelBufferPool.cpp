// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelBufferPool.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelBufferRef);

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, int32, GVoxelBufferPoolMaxUploadSize, 256 * 1024 * 1024,
	"voxel.BufferPool.MaxUploadSize",
	"Max upload size during a single upload");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBufferRef::FVoxelBufferRef(
	FVoxelBufferPool& Pool,
	const int32 PoolIndex,
	const int64 Index,
	const int64 Num)
	: WeakPool(Pool.AsWeak())
	, PoolIndex(PoolIndex)
	, Index(Index)
	, PrivateNum(Num)
{
	const int64 UsedMemory = PrivateNum * Pool.BytesPerElement;
	const int64 PaddingMemory = Pool.GetPoolSize(PoolIndex) * Pool.BytesPerElement - UsedMemory;

	Pool.UsedMemory.Add(UsedMemory);
	Pool.PaddingMemory.Add(PaddingMemory);
}

FVoxelBufferRef::~FVoxelBufferRef()
{
	const TSharedPtr<FVoxelBufferPool> Pool = WeakPool.Pin();
	if (!Pool)
	{
		return;
	}

	const int64 UsedMemory = PrivateNum * Pool->BytesPerElement;
	const int64 PaddingMemory = Pool->GetPoolSize(PoolIndex) * Pool->BytesPerElement - UsedMemory;

	Pool->UsedMemory.Subtract(UsedMemory);
	Pool->PaddingMemory.Subtract(PaddingMemory);

	VOXEL_SCOPE_LOCK(Pool->PoolIndexToPool_CriticalSection);
	Pool->PoolIndexToPool_RequiresLock[PoolIndex].Free(Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBufferPool::FVoxelBufferPool(
	const int32 BytesPerElement,
	const EPixelFormat PixelFormat,
	const TCHAR* BufferName)
	: BytesPerElement(BytesPerElement)
	, PixelFormat(PixelFormat)
	, BufferName(BufferName)
	, AllocatedMemory_Name(FString(BufferName) + " Allocated Memory")
	, UsedMemory_Name(FString(BufferName) + " Used Memory")
	, PaddingMemory_Name(FString(BufferName) + " Padding Memory")
{
	ensure(BytesPerElement % GPixelFormats[PixelFormat].BlockBytes == 0);

	const int32 MaxPoolIndex = NumToPoolIndex(MAX_uint32);

	for (int32 PoolIndex = 0; PoolIndex <= MaxPoolIndex; PoolIndex++)
	{
		PoolIndexToPool_RequiresLock.Add(FAllocationPool(PoolIndex));
	}

	Voxel_AddAmountToDynamicStat(BufferName, AllocatedMemory.Get());
}

FVoxelBufferPool::~FVoxelBufferPool()
{
	Voxel_AddAmountToDynamicStat(AllocatedMemory_Name, -AllocatedMemory_Reported.Get());
	Voxel_AddAmountToDynamicStat(UsedMemory_Name, -UsedMemory_Reported.Get());
	Voxel_AddAmountToDynamicStat(PaddingMemory_Name, -PaddingMemory_Reported.Get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferPool::UpdateStats()
{
	const int64 AllocatedMemoryNew = AllocatedMemory.Get();
	const int64 UsedMemoryNew = UsedMemory.Get();
	const int64 PaddingMemoryNew = PaddingMemory.Get();

	const int64 AllocatedMemoryOld = AllocatedMemory_Reported.Exchange_ReturnOld(AllocatedMemoryNew);
	const int64 UsedMemoryOld = UsedMemory_Reported.Exchange_ReturnOld(UsedMemoryNew);
	const int64 PaddingMemoryOld = PaddingMemory_Reported.Exchange_ReturnOld(PaddingMemoryNew);

	Voxel_AddAmountToDynamicStat(AllocatedMemory_Name, AllocatedMemoryNew - AllocatedMemoryOld);
	Voxel_AddAmountToDynamicStat(UsedMemory_Name, UsedMemoryNew - UsedMemoryOld);
	Voxel_AddAmountToDynamicStat(PaddingMemory_Name, PaddingMemoryNew - PaddingMemoryOld);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelBufferRef> FVoxelBufferPool::Allocate_AnyThread(const int64 Num)
{
	const int32 PoolIndex = NumToPoolIndex(Num);

	const int64 Index = INLINE_LAMBDA
	{
		VOXEL_SCOPE_LOCK(PoolIndexToPool_CriticalSection);
		return PoolIndexToPool_RequiresLock[PoolIndex].Allocate(*this);
	};

	return MakeVoxelShared<FVoxelBufferRef>(
		*this,
		PoolIndex,
		Index,
		Num);
}

TVoxelFuture<FVoxelBufferRef> FVoxelBufferPool::Upload_AnyThread(
	const FSharedVoidPtr& Owner,
	const TConstVoxelArrayView64<uint8> Data,
	const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef)
{
	checkVoxelSlow(Data.Num() % BytesPerElement == 0);
	checkVoxelSlow(!ExistingBufferRef || ExistingBufferRef->WeakPool == AsWeak());

	const TVoxelPromise<FVoxelBufferRef> Promise;

	UploadQueue.Enqueue(FUpload
	{
		Owner,
		Data,
		ExistingBufferRef,
		MakeSharedCopy(Promise)
	});

	CheckUploadQueue_AnyThread();

	return Promise.GetFuture();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelBufferPool::FAllocationPool::Allocate(FVoxelBufferPool& Pool)
{
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (FreeIndices_RequiresLock.Num() > 0)
		{
			return FreeIndices_RequiresLock.Pop();
		}
	}

	const int64 Index = Pool.BufferCount.Add_ReturnOld(PoolSize);
	ensure(Pool.BufferCount.Get() < MAX_uint32);
	return Index;
}

void FVoxelBufferPool::FAllocationPool::Free(const int64 Index)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	FreeIndices_RequiresLock.Add(Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferPool::CheckUploadQueue_AnyThread()
{
	if (UploadQueue.IsEmpty())
	{
		return;
	}

	if (IsProcessingUploads.Exchange_ReturnOld(true) == true)
	{
		return;
	}

	Voxel::AsyncTask(MakeWeakPtrLambda(this, [this]
	{
		ensure(IsProcessingUploads.Get());
		TVoxelArray<FCopyInfo> CopyInfos = ProcessUploads_AnyThread();
		ensure(IsProcessingUploads.Get());

		Voxel::RenderTask(MakeWeakPtrLambda(this, [this, CopyInfos = MoveTemp(CopyInfos)](FRHICommandList& RHICmdList)
		{
			ensure(IsProcessingUploads.Get());
			ProcessCopies_RenderThread(RHICmdList, CopyInfos);
			ensure(IsProcessingUploads.Get());

			ensure(IsProcessingUploads.Exchange_ReturnOld(false));

			CheckUploadQueue_AnyThread();
		}));
	}));
}

TVoxelArray<FVoxelBufferPool::FCopyInfo> FVoxelBufferPool::ProcessUploads_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(IsProcessingUploads.Get());

	int64 NumBytes = 0;
	TVoxelArray<FUpload> Uploads;
	{
		const int64 MaxNumBytes = FMath::Min(MAX_int32, GVoxelBufferPoolMaxUploadSize);

		while (true)
		{
			FUpload* Upload = UploadQueue.Peek();
			if (!Upload)
			{
				break;
			}

			if (NumBytes + Upload->NumBytes() >= MaxNumBytes)
			{
				// Only break if this upload can actually fit within the limit
				if (Upload->NumBytes() < MaxNumBytes)
				{
					break;
				}
			}

			NumBytes += Upload->NumBytes();
			Uploads.Add(MoveTemp(*Upload));

			check(UploadQueue.Pop());
		}

		// Will trigger if an upload is bigger than MaxNumBytes
		ensureVoxelSlow(NumBytes <= MaxNumBytes);
	}
	checkVoxelSlow(NumBytes <= MAX_int32);
	checkVoxelSlow(NumBytes % BytesPerElement == 0);

	VOXEL_SCOPE_COUNTER_FORMAT("Num=%lldB", NumBytes);

	// FD3D12DynamicRHI::CreateD3D12Buffer doesn't need RHICmdList
	FRHICommandListBase& DummyRHICmdList = *reinterpret_cast<FRHICommandListBase*>(1);

	FBufferRHIRef UploadBuffer;
	{
		VOXEL_SCOPE_COUNTER("RHICreateBuffer");

		const FRHIBufferDesc BufferDesc(
			NumBytes,
			BytesPerElement,
			EBufferUsageFlags::Dynamic);

		FRHIResourceCreateInfo CreateInfo(TEXT("VoxelUpload"));

		UploadBuffer = GDynamicRHI->RHICreateBuffer(
			DummyRHICmdList,
			BufferDesc,
			ERHIAccess::CopySrc,
			CreateInfo);
	}

	TVoxelArrayView64<uint8> BufferView;
	{
		VOXEL_SCOPE_COUNTER("RHILockBuffer");

		void* Data = GDynamicRHI->RHILockBuffer(
			DummyRHICmdList,
			UploadBuffer,
			0,
			NumBytes,
			RLM_WriteOnly);

		BufferView = TVoxelArrayView64<uint8>(static_cast<uint8*>(Data), NumBytes);
	}

	{
		VOXEL_SCOPE_COUNTER_FORMAT("Copy %lldB", NumBytes);

		int64 Index = 0;
		for (const FUpload& Upload : Uploads)
		{
			FVoxelUtilities::Memcpy(
				BufferView.Slice(Index, Upload.NumBytes()),
				Upload.Data);

			Index += Upload.NumBytes();
		}
		checkVoxelSlow(Index == NumBytes);
	}

	{
		VOXEL_SCOPE_COUNTER("RHIUnlockBuffer");
		GDynamicRHI->RHIUnlockBuffer(DummyRHICmdList, UploadBuffer);
	}

	TVoxelArray<FCopyInfo> CopyInfos;
	{
		VOXEL_SCOPE_COUNTER("CopyInfos");

		int64 UploadIndex = 0;
		for (const FUpload& Upload : Uploads)
		{
			checkVoxelSlow(Upload.NumBytes() % BytesPerElement == 0);
			const int64 Num = Upload.NumBytes() / BytesPerElement;

			const TSharedRef<FVoxelBufferRef> BufferRef =
				Upload.ExistingBufferRef.IsValid()
				? Upload.ExistingBufferRef.ToSharedRef()
				: Allocate_AnyThread(Num);

			checkVoxelSlow(BufferRef->WeakPool == AsWeak());
			checkVoxelSlow(BufferRef->Num() == Num);

			CopyInfos.Add(FCopyInfo
			{
				BufferRef,
				Upload.BufferRefPromise,
				UploadBuffer,
				UploadIndex
			});

			UploadIndex += Num;
		}
		checkVoxelSlow(UploadIndex * BytesPerElement == NumBytes);
	}

	return CopyInfos;
}

void FVoxelBufferPool::ProcessCopies_RenderThread(
	FRHICommandList& RHICmdList,
	const TConstVoxelArrayView<FCopyInfo> CopyInfos)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInRenderingThread());
	ensure(CopyInfos.Num() > 0);

	// Do this after dequeuing all copies to make sure we allocate a big enough buffer for them
	const int64 Num = BufferCount.Get();

	if (!BufferRHI_RenderThread ||
		int64(BufferRHI_RenderThread->GetSize()) < Num * BytesPerElement)
	{
		VOXEL_SCOPE_COUNTER("Create buffer");

		const FBufferRHIRef OldBufferRHI = BufferRHI_RenderThread;

		FRHIResourceCreateInfo CreateInfo(BufferName);

		int64 AllocatedNum = FMath::RoundUpToPowerOfTwo64(Num);
		ensure(AllocatedNum <= 1 << 30);
		AllocatedMemory.Set(AllocatedNum);

		// Avoid DX12 resource pooling to prevent crashes when using CopyBufferRegion
		AllocatedNum = FMath::Max<int64>(32 * 1024 * 1024, AllocatedNum);

		BufferRHI_RenderThread = RHICmdList.CreateBuffer(
			AllocatedNum * BytesPerElement,
			EBufferUsageFlags::ShaderResource |
			EBufferUsageFlags::Static,
			BytesPerElement,
			ERHIAccess::Unknown,
			CreateInfo);

		BufferSRV_RenderThread = RHICmdList.CreateShaderResourceView(
			BufferRHI_RenderThread,
			GPixelFormats[PixelFormat].BlockBytes,
			PixelFormat);

		if (OldBufferRHI)
		{
			VOXEL_SCOPE_COUNTER("CopyBufferRegion");

			RHICmdList.CopyBufferRegion(
				BufferRHI_RenderThread,
				0,
				OldBufferRHI,
				0,
				OldBufferRHI->GetSize());
		}
	}

	for (const FCopyInfo& CopyInfo : CopyInfos)
	{
		VOXEL_SCOPE_COUNTER("CopyBufferRegion");
		checkVoxelSlow(CopyInfo.BufferRef->WeakPool == AsWeak());

		RHICmdList.CopyBufferRegion(
			BufferRHI_RenderThread,
			CopyInfo.BufferRef->Index * BytesPerElement,
			CopyInfo.SourceBuffer,
			CopyInfo.SourceOffset * BytesPerElement,
			CopyInfo.BufferRef->Num() * BytesPerElement);

		// Upload is complete: notify caller
		CopyInfo.BufferRefPromise->Set(CopyInfo.BufferRef.ToSharedRef());
	}
}