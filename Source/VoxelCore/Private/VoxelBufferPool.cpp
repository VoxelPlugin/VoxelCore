// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelBufferPool.h"
#include "VoxelTaskContext.h"
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
	if (IsOutOfMemory())
	{
		return;
	}

	const int64 UsedMemory = PrivateNum * Pool.BytesPerElement;
	const int64 PaddingMemory = Pool.GetPoolSize(PoolIndex) * Pool.BytesPerElement - UsedMemory;

	Pool.UsedMemory.Add(UsedMemory);
	Pool.PaddingMemory.Add(PaddingMemory);
}

FVoxelBufferRef::~FVoxelBufferRef()
{
	if (IsOutOfMemory())
	{
		return;
	}

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
	const FString& BufferName)
	: BytesPerElement(BytesPerElement)
	, PixelFormat(PixelFormat)
	, BufferName(BufferName)
	, AllocatedMemory_Name(BufferName + " Allocated Memory")
	, UsedMemory_Name(BufferName + " Used Memory")
	, PaddingMemory_Name(BufferName + " Padding Memory")
{
	ensure(BytesPerElement % GPixelFormats[PixelFormat].BlockBytes == 0);

	const int32 MaxPoolIndex = NumToPoolIndex(MAX_uint32);

	for (int32 PoolIndex = 0; PoolIndex <= MaxPoolIndex; PoolIndex++)
	{
		PoolIndexToPool_RequiresLock.Add(FAllocationPool(PoolIndex));
	}
}

FVoxelBufferPoolBase::~FVoxelBufferPoolBase()
{
	Voxel_AddAmountToDynamicMemoryStat(AllocatedMemory_Name, -AllocatedMemory_Reported.Get());
	Voxel_AddAmountToDynamicMemoryStat(UsedMemory_Name, -UsedMemory_Reported.Get());
	Voxel_AddAmountToDynamicMemoryStat(PaddingMemory_Name, -PaddingMemory_Reported.Get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelBufferPoolBase::UpdateStats()
{
	const int64 AllocatedMemoryNew = AllocatedMemory.Get();
	const int64 UsedMemoryNew = UsedMemory.Get();
	const int64 PaddingMemoryNew = PaddingMemory.Get();

	const int64 AllocatedMemoryOld = AllocatedMemory_Reported.Set_ReturnOld(AllocatedMemoryNew);
	const int64 UsedMemoryOld = UsedMemory_Reported.Set_ReturnOld(UsedMemoryNew);
	const int64 PaddingMemoryOld = PaddingMemory_Reported.Set_ReturnOld(PaddingMemoryNew);

	Voxel_AddAmountToDynamicMemoryStat(AllocatedMemory_Name, AllocatedMemoryNew - AllocatedMemoryOld);
	Voxel_AddAmountToDynamicMemoryStat(UsedMemory_Name, UsedMemoryNew - UsedMemoryOld);
	Voxel_AddAmountToDynamicMemoryStat(PaddingMemory_Name, PaddingMemoryNew - PaddingMemoryOld);
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

	if (Index == -1)
	{
		return MakeShared<FVoxelBufferRef>(
			*this,
			-1,
			0,
			Num);
	}

	return MakeShared<FVoxelBufferRef>(
		*this,
		PoolIndex,
		Index,
		Num);
}

FVoxelBufferUpload FVoxelBufferPoolBase::Upload_AnyThread(
	const FSharedVoidPtr& Owner,
	const TConstVoxelArrayView64<uint8> Data,
	const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(Data.Num() > 0);
	checkVoxelSlow(Data.Num() % BytesPerElement == 0);

	FVoxelTaskContext& ReturnContext = FVoxelTaskScope::GetContext();
	FVoxelTaskScope Scope(*GVoxelGlobalTaskContext);

	const int64 Num = Data.Num() / BytesPerElement;

	checkVoxelSlow(!ExistingBufferRef || ExistingBufferRef->WeakPool == AsWeak());
	checkVoxelSlow(!ExistingBufferRef || ExistingBufferRef->Num() == Num);

	TSharedPtr<FVoxelBufferRef> BufferRef = ExistingBufferRef;
	if (!BufferRef)
	{
		BufferRef = Allocate_AnyThread(Num);
	}

	if (BufferRef->IsOutOfMemory())
	{
		if (OnOutOfMemory.IsBound())
		{
			OnOutOfMemory.Broadcast();
		}
		else
		{
			VOXEL_MESSAGE(Error, "Out of memory: {0}", BufferName);
		}

		return FVoxelBufferUpload
		{
			{},
			BufferRef.ToSharedRef()
		};
	}

	const FVoxelPromise Promise;

	UploadQueue.Enqueue(FUpload
	{
		Owner,
		Data,
		BufferRef,
		MakeSharedCopy(Promise)
	});

	CheckUploadQueue_AnyThread();

	return FVoxelBufferUpload
	{
		ReturnContext.Wrap(Promise),
		BufferRef.ToSharedRef()
	};
}

FVoxelBufferUpload FVoxelBufferPoolBase::Upload_AnyThread(
	TVoxelArray<uint8> Data,
	const TSharedPtr<FVoxelBufferRef>& ExistingBufferRef)
{
	const TSharedRef<TVoxelArray<uint8>> SharedData = MakeSharedCopy(MoveTemp(Data));

	return this->Upload_AnyThread(
		MakeSharedVoidRef(SharedData),
		MakeByteVoxelArrayView(*SharedData),
		ExistingBufferRef);
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

	VOXEL_SCOPE_LOCK(Pool.BufferCount_CriticalSection);

	if (Pool.BufferCount.Get() + PoolSize > Pool.GetMaxAllocatedNum())
	{
		return -1;
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

int64 FVoxelBufferPool::GetMaxAllocatedNum() const
{
	return FMath::DivideAndRoundDown<int64>(MAX_uint32, BytesPerElement);
}

void FVoxelBufferPool::CheckUploadQueue_AnyThread()
{
	if (UploadQueue.IsEmpty())
	{
		return;
	}

	if (IsProcessingUploads.Set_ReturnOld(true) == true)
	{
		return;
	}

	Voxel::AsyncTask(MakeWeakPtrLambda(this, [this]
	{
		ensure(IsProcessingUploads.Get());

		ProcessUploads_AnyThread().Then_AnyThread(MakeWeakPtrLambda(this, [this]
		{
			ensure(IsProcessingUploads.Set_ReturnOld(false));

			CheckUploadQueue_AnyThread();
		}));
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture FVoxelBufferPool::ProcessUploads_AnyThread()
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

	UpdateStats();

	return ProcessUploadsImpl_AnyThread(MoveTemp(Uploads)).Then_AnyThread(MakeWeakPtrLambda(this, [this]
	{
		UpdateStats();
	}));
}

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

#if VOXEL_ENGINE_VERSION >= 506
		check(false);
#else
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
#endif
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
				Upload.Promise,
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
	const int64 Num = FMath::Min(BufferCount.Get(), GetMaxAllocatedNum());

	int64 AllocatedNum = FMath::RoundUpToPowerOfTwo64(Num);

	// Avoid DX12 resource pooling to prevent crashes when using CopyBufferRegion
	AllocatedNum = FMath::Max<int64>(32 * 1024 * 1024, AllocatedNum);

	ensure(AllocatedNum <= 1 << 30);
	AllocatedMemory.Set(AllocatedNum * BytesPerElement);

	if (!BufferRHI_RenderThread ||
		int64(BufferRHI_RenderThread->GetSize()) < AllocatedNum * BytesPerElement)
	{
		VOXEL_SCOPE_COUNTER("Create buffer");

		const FBufferRHIRef OldBufferRHI = BufferRHI_RenderThread;

#if VOXEL_ENGINE_VERSION >= 506
		check(false);
#else
		FRHIResourceCreateInfo CreateInfo(*BufferName);

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
#endif

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
		CopyInfo.Promise->Set();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelTextureBufferPoolStatics
{
	FVoxelCriticalSection CriticalSection;
	TVoxelSet<FVoxelTextureBufferPool*> Pools;
};
FVoxelTextureBufferPoolStatics GVoxelTextureBufferPoolStatics;

class FVoxelTextureBufferPoolSingleton : public FVoxelSingleton
{
public:
	//~ Begin FVoxelSingleton Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();
		VOXEL_SCOPE_LOCK(GVoxelTextureBufferPoolStatics.CriticalSection);

		for (FVoxelTextureBufferPool* Pool : GVoxelTextureBufferPoolStatics.Pools)
		{
			Pool->Tick();
		}
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		VOXEL_FUNCTION_COUNTER();
		VOXEL_SCOPE_LOCK(GVoxelTextureBufferPoolStatics.CriticalSection);

		for (FVoxelTextureBufferPool* Pool : GVoxelTextureBufferPoolStatics.Pools)
		{
			Pool->AddReferencedObjects(Collector);
		}
	}
	//~ End FVoxelSingleton Interface
};
FVoxelTextureBufferPoolSingleton* GVoxelTextureBufferPoolSingleton = new FVoxelTextureBufferPoolSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTextureBufferPool::FVoxelTextureBufferPool(
	const int32 BytesPerElement,
	const EPixelFormat PixelFormat,
	const FString& BufferName,
	const int32 MaxTextureSize)
	: FVoxelBufferPoolBase(BytesPerElement, PixelFormat, BufferName)
	, MaxTextureSize(MaxTextureSize)
{
	check(FMath::IsPowerOfTwo(MaxTextureSize));
	check(FMath::Square(MaxTextureSize) * BytesPerElement <= MAX_uint32);

	VOXEL_SCOPE_LOCK(GVoxelTextureBufferPoolStatics.CriticalSection);
	GVoxelTextureBufferPoolStatics.Pools.Add_CheckNew(this);
}

FVoxelTextureBufferPool::~FVoxelTextureBufferPool()
{
	VOXEL_SCOPE_LOCK(GVoxelTextureBufferPoolStatics.CriticalSection);
	GVoxelTextureBufferPoolStatics.Pools.Remove_Ensure(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int64 FVoxelTextureBufferPool::GetMaxAllocatedNum() const
{
	return FMath::Square(MaxTextureSize);
}

void FVoxelTextureBufferPool::CheckUploadQueue_AnyThread()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTextureBufferPool::Tick()
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<FUpload> Uploads;
	{
		FUpload Upload;
		while (UploadQueue.Dequeue(Upload))
		{
			Uploads.Add(MoveTemp(Upload));
		}
	}

	if (Uploads.Num() == 0 ||
		IsEngineExitRequested())
	{
		return;
	}

	UpdateStats();

	UTexture2D* OldTexture = Texture_GameThread;
	{
		// Do this after dequeuing all copies to make sure we allocate a big enough buffer for them
		const int64 Num = FMath::Min(BufferCount.Get(), GetMaxAllocatedNum());
		const int32 Size = FMath::Max<int32>(1024, FMath::RoundUpToPowerOfTwo(FMath::CeilToInt(FMath::Sqrt(double(Num)))));

		{
			const int64 AllocatedNum = FMath::Square<int64>(Size);
			ensure(AllocatedNum <= 1 << 30);
			AllocatedMemory.Set(AllocatedNum * BytesPerElement);
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

	{
		UTexture2D* NewTexture = Texture_GameThread;
		if (!ensure(NewTexture))
		{
			return;
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

			Voxel::RenderTask([OldResource, NewResource, Scale](FRHICommandListImmediate& RHICmdList)
			{
				VOXEL_SCOPE_COUNTER("FVoxelTextureBufferPool Reallocate");

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
	}
	check(Texture_GameThread);

	FTextureResource* Resource = Texture_GameThread->GetResource();
	if (!ensure(Resource))
	{
		return;
	}

	Voxel::RenderTask(MakeWeakPtrLambda(this, [this, Resource, Uploads = MoveTemp(Uploads)]
	{
		VOXEL_FUNCTION_COUNTER();

		FRHITexture* TextureRHI = Resource->GetTexture2DRHI();
		if (!ensure(TextureRHI))
		{
			return;
		}

		TextureRHI_RenderThread = TextureRHI;

		const int32 TextureSize = TextureRHI->GetSizeX();

		for (const FUpload& Upload : Uploads)
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
			Upload.Promise->Set();
		}

		UpdateStats();
	}));
}

void FVoxelTextureBufferPool::AddReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	Collector.AddReferencedObject(Texture_GameThread);
}