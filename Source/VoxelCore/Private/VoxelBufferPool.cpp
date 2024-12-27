// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelBufferPool.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelBufferRef);

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, int32, GVoxelBufferPoolMaxUploadSize, 256 * 1024 * 1024,
	"voxel.BufferPool.MaxUploadSize",
	"Max upload size during a single upload");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBufferRef::FVoxelBufferRef(
	FVoxelBufferPoolBase& Pool,
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
	const TSharedPtr<FVoxelBufferPoolBase> Pool = WeakPool.Pin();
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

FVoxelBufferPoolBase::FVoxelBufferPoolBase(
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

FVoxelBufferPoolBase::~FVoxelBufferPoolBase()
{
	Voxel_AddAmountToDynamicStat(AllocatedMemory_Name, -AllocatedMemory_Reported.Get());
	Voxel_AddAmountToDynamicStat(UsedMemory_Name, -UsedMemory_Reported.Get());
	Voxel_AddAmountToDynamicStat(PaddingMemory_Name, -PaddingMemory_Reported.Get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferPoolBase::UpdateStats()
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

TSharedRef<FVoxelBufferRef> FVoxelBufferPoolBase::Allocate_AnyThread(const int64 Num)
{
	const int32 PoolIndex = NumToPoolIndex(Num);

	const int64 Index = INLINE_LAMBDA
	{
		VOXEL_SCOPE_LOCK(PoolIndexToPool_CriticalSection);
		return PoolIndexToPool_RequiresLock[PoolIndex].Allocate(*this);
	};

	return MakeShared<FVoxelBufferRef>(
		*this,
		PoolIndex,
		Index,
		Num);
}

TVoxelFuture<FVoxelBufferRef> FVoxelBufferPoolBase::Upload_AnyThread(
	const FSharedVoidPtr& Owner,
	const TConstVoxelArrayView64<uint8> Data,
	const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef)
{
	ensure(Data.Num() > 0);
	checkVoxelSlow(Data.Num() % BytesPerElement == 0);

	const int64 Num = Data.Num() / BytesPerElement;

	checkVoxelSlow(!ExistingBufferRef || ExistingBufferRef->WeakPool == AsWeak());
	checkVoxelSlow(!ExistingBufferRef || ExistingBufferRef->Num() == Num);

	TSharedPtr<FVoxelBufferRef> BufferRef = ExistingBufferRef;
	if (!BufferRef)
	{
		BufferRef = Allocate_AnyThread(Num);
	}

	TVoxelPromise<FVoxelBufferRef> Promise;

	UploadQueue.Enqueue(FUpload
	{
		Owner,
		Data,
		BufferRef,
		MakeSharedCopy(Promise)
	});

	CheckUploadQueue_AnyThread();

	return Promise;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelBufferPoolBase::FAllocationPool::Allocate(FVoxelBufferPoolBase& Pool)
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

void FVoxelBufferPoolBase::FAllocationPool::Free(const int64 Index)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	FreeIndices_RequiresLock.Add(Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferPoolBase::CheckUploadQueue_AnyThread()
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

		ProcessUploads_AnyThread().Then_AnyThread(MakeWeakPtrLambda(this, [this]
		{
			ensure(IsProcessingUploads.Exchange_ReturnOld(false));

			CheckUploadQueue_AnyThread();
		}));
	}));
}

FVoxelFuture FVoxelBufferPoolBase::ProcessUploads_AnyThread()
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

			ensure(UploadQueue.Pop());
		}

		// Will trigger if an upload is bigger than MaxNumBytes
		ensureVoxelSlow(NumBytes <= MaxNumBytes);
	}
	checkVoxelSlow(NumBytes <= MAX_int32);
	checkVoxelSlow(NumBytes % BytesPerElement == 0);

	return ProcessUploadsImpl_AnyThread(MoveTemp(Uploads));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture FVoxelBufferPool::ProcessUploadsImpl_AnyThread(TVoxelArray<FUpload>&& Uploads)
{
	VOXEL_FUNCTION_COUNTER();
	checkVoxelSlow(IsProcessingUploads.Get());

	int64 NumBytes = 0;
	for (const FUpload& Upload : Uploads)
	{
		NumBytes += Upload.NumBytes();
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
			ERHIAccess::CopySrc | ERHIAccess::SRVCompute,
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

		CopyInfos.Reserve(Uploads.Num());

		int64 UploadIndex = 0;
		for (const FUpload& Upload : Uploads)
		{
			checkVoxelSlow(Upload.NumBytes() % BytesPerElement == 0);
			const int64 Num = Upload.NumBytes() / BytesPerElement;

			checkVoxelSlow(Upload.BufferRef->WeakPool == AsWeak());
			checkVoxelSlow(Upload.BufferRef->Num() == Num);

			CopyInfos.Add_EnsureNoGrow(FCopyInfo
			{
				Upload.BufferRef,
				Upload.BufferRefPromise,
				UploadBuffer,
				UploadIndex
			});

			UploadIndex += Num;
		}
		checkVoxelSlow(UploadIndex * BytesPerElement == NumBytes);
	}

	return Voxel::RenderTask(MakeWeakPtrLambda(this, [this, CopyInfos = MoveTemp(CopyInfos)](FRHICommandList& RHICmdList)
	{
		ProcessCopies_RenderThread(RHICmdList, CopyInfos);
	}));
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

	int64 AllocatedNum = FMath::RoundUpToPowerOfTwo64(Num);

	// Avoid DX12 resource pooling to prevent crashes when using CopyBufferRegion
	AllocatedNum = FMath::Max<int64>(32 * 1024 * 1024, AllocatedNum);

	ensure(AllocatedNum <= 1 << 30);
	AllocatedMemory.Set(AllocatedNum);

	if (!BufferRHI_RenderThread ||
		int64(BufferRHI_RenderThread->GetSize()) < AllocatedNum * BytesPerElement)
	{
		VOXEL_SCOPE_COUNTER("Create buffer");

		const FBufferRHIRef OldBufferRHI = BufferRHI_RenderThread;

		FRHIResourceCreateInfo CreateInfo(BufferName);

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTextureBufferPool::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	Collector.AddReferencedObject(Texture_GameThread);
}

FVoxelFuture FVoxelTextureBufferPool::ProcessUploadsImpl_AnyThread(TVoxelArray<FUpload>&& Uploads)
{
	VOXEL_FUNCTION_COUNTER();

	return Voxel::GameTask(MakeWeakPtrLambda(this, [this, Uploads = MakeSharedCopy(MoveTemp(Uploads))]
	{
		if (IsEngineExitRequested())
		{
			return FVoxelFuture();
		}

		UTexture2D* OldTexture = Texture_GameThread;
		{
			// Do this after dequeuing all copies to make sure we allocate a big enough buffer for them
			const int64 Num = BufferCount.Get();
			const int32 Size = FMath::Max<int32>(1024, FMath::RoundUpToPowerOfTwo(FMath::CeilToInt(FMath::Sqrt(double(Num)))));

			{
				const int64 AllocatedNum = FMath::Square<int64>(Size);
				ensure(AllocatedNum <= 1 << 30);
				AllocatedMemory.Set(AllocatedNum);
			}

			if (!Texture_GameThread ||
				Texture_GameThread->GetSizeX() != Size)
			{
				Texture_GameThread = FVoxelTextureUtilities::CreateTexture2D(
					FName(BufferName + FString("_Texture")),
					Size,
					Size,
					false,
					TF_Default,
					PixelFormat);

				FVoxelTextureUtilities::RemoveBulkData(Texture_GameThread);
			}
		}

		UTexture2D* NewTexture = Texture_GameThread;
		if (!ensure(NewTexture))
		{
			return FVoxelFuture();
		}

		if (OldTexture &&
			OldTexture != NewTexture &&
			ensure(NewTexture->GetSizeX() % OldTexture->GetSizeX() == 0) &&
			ensure(NewTexture->GetSizeY() % OldTexture->GetSizeY() == 0))
		{
			FTextureResource* OldResource = OldTexture->GetResource();
			FTextureResource* NewResource = NewTexture->GetResource();

			const int32 Scale = NewTexture->GetSizeX() / OldTexture->GetSizeX();
			ensure(Scale == NewTexture->GetSizeY() / OldTexture->GetSizeY());
			ensure(Scale > 1);
			ensure(FMath::IsPowerOfTwo(Scale));

			VOXEL_ENQUEUE_RENDER_COMMAND(FVoxelTextureBufferPool_Reallocate)([OldResource, NewResource, Scale](FRHICommandListImmediate& RHICmdList)
			{
				const int32 SizeX = OldResource->GetSizeX();
				const int32 SizeY = OldResource->GetSizeY();

				for (int32 Row = 0; Row < SizeY; Row++)
				{
					FRHICopyTextureInfo CopyInfo;
					CopyInfo.Size = FIntVector(SizeX, 1, 1);
					CopyInfo.SourcePosition.X = 0;
					CopyInfo.SourcePosition.Y = Row;
					CopyInfo.DestPosition.X = (Row % Scale) * SizeX;
					CopyInfo.DestPosition.Y = Row / Scale;

					RHICmdList.CopyTexture(
						OldResource->GetTextureRHI(),
						NewResource->GetTextureRHI(),
						CopyInfo);
				}
			});
		}

		check(Texture_GameThread);

		FTextureResource* Resource = Texture_GameThread->GetResource();
		if (!ensure(Resource))
		{
			return FVoxelFuture();
		}

		return Voxel::RenderTask(MakeWeakPtrLambda(this, [this, Resource, Uploads]
		{
			FRHITexture* TextureRHI = Resource->GetTexture2DRHI();
			if (!ensure(TextureRHI))
			{
				return;
			}

			TextureRHI_RenderThread = TextureRHI;

			const int32 TextureSize = TextureRHI->GetSizeX();

			for (const FUpload& Upload : *Uploads)
			{
				checkVoxelSlow(Upload.NumBytes() % BytesPerElement == 0);
				const int64 Num = Upload.NumBytes() / BytesPerElement;

				checkVoxelSlow(Upload.BufferRef->WeakPool == AsWeak());
				checkVoxelSlow(Upload.BufferRef->Num() == Num);

				int64 Offset = Upload.BufferRef->GetIndex();
				TConstVoxelArrayView64<uint8> Data = Upload.Data;

				while (Data.Num() > 0)
				{
					check(Data.Num() % BytesPerElement == 0);

					const int64 NumToCopy = FMath::Min(Data.Num() / BytesPerElement, TextureSize - (Offset % TextureSize));

					const FUpdateTextureRegion2D UpdateRegion(
						Offset % TextureSize,
						Offset / TextureSize,
						0,
						0,
						NumToCopy,
						1);

					RHIUpdateTexture2D_Safe(
						TextureRHI,
						0,
						UpdateRegion,
						NumToCopy * BytesPerElement,
						Data.LeftOf(NumToCopy * BytesPerElement));

					Offset += NumToCopy;
					Data = Data.RightOf(NumToCopy * BytesPerElement);
				}

				// Upload is complete: notify caller
				Upload.BufferRefPromise->Set(Upload.BufferRef.ToSharedRef());
			}
		}));
	}));
}