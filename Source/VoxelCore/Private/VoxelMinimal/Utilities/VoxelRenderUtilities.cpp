// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Shader.h"
#include "ShaderCore.h"
#include "Async/Async.h"
#include "GlobalShader.h"
#include "SceneManagement.h"
#include "TextureResource.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "Materials/MaterialRenderProxy.h"

FRDGBuilder* FVoxelRDGBuilderScope::StaticBuilder = nullptr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelRDGExternalBuffer> FVoxelRDGExternalBuffer::Create(
	const TRefCountPtr<FRDGPooledBuffer>& PooledBuffer,
	const EPixelFormat Format,
	const TCHAR* Name)
{
	check(PooledBuffer);

	const TSharedRef<FVoxelRDGExternalBuffer> Result = MakeVoxelShared<FVoxelRDGExternalBuffer>();
	Result->Type = EType::VertexBuffer;
	Result->Name = Name;
	Result->Format = Format;
	Result->BytesPerElement = PooledBuffer->Desc.BytesPerElement;
	Result->NumElements = PooledBuffer->Desc.NumElements;
	Result->PooledBuffer = PooledBuffer;
	return Result;
}

TSharedRef<FVoxelRDGExternalBuffer> FVoxelRDGExternalBuffer::Create(
	FRHICommandListBase& RHICmdList,
	const int64 BytesPerElement,
	const int64 NumElements,
	const EPixelFormat Format,
	const TCHAR* Name,
	const EBufferUsageFlags AdditionalFlags,
	FResourceArrayInterface* ResourceArray)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(NumElements <= MAX_int32);
	ensure(NumElements * BytesPerElement <= MAX_uint32);
	ensure(Format != PF_Unknown);
	ensure(GPixelFormats[Format].BlockBytes == BytesPerElement);
	ensure(!(AdditionalFlags & BUF_Dynamic));
	ensure(!(AdditionalFlags & BUF_Volatile));
	ensure(!ResourceArray || ResourceArray->GetResourceDataSize() == BytesPerElement * NumElements);

	FRDGBufferDesc Desc;
	Desc.Usage = BUF_Static | BUF_UnorderedAccess | BUF_ShaderResource | BUF_VertexBuffer | AdditionalFlags;
	Desc.BytesPerElement = BytesPerElement;
	Desc.NumElements = NumElements;
	ensure(Desc.NumElements <= MAX_int32);
	ensure(Desc.GetSize() < MAX_uint32);

	const TSharedRef<FVoxelRDGExternalBuffer> Result = MakeVoxelShared<FVoxelRDGExternalBuffer>();
	Result->Type = EType::VertexBuffer;
	Result->Name = Name;
	Result->Format = Format;
	Result->BytesPerElement = BytesPerElement;
	Result->NumElements = NumElements;

	FRHIResourceCreateInfo CreateInfo(Name, ResourceArray);

	FBufferRHIRef Buffer;
	if (IsInRenderingThread())
	{
		Buffer = RHICmdList.CreateBuffer(
			Desc.GetSize(),
			Desc.Usage,
			0,
			ERHIAccess::Unknown,
			CreateInfo);
	}
	else
	{
		// Buggy
		check(false);
	}

	Result->PooledBuffer = new FRDGPooledBuffer(Buffer, Desc, Desc.GetSize(), Name);

	return Result;
}

TSharedRef<FVoxelRDGExternalBuffer> FVoxelRDGExternalBuffer::CreateStructured(
	FRHICommandListBase& RHICmdList,
	const int64 BytesPerElement,
	const int64 NumElements,
	const TCHAR* Name,
	const EBufferUsageFlags AdditionalFlags,
	FResourceArrayInterface* ResourceArray)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(NumElements <= MAX_int32);
	ensure(NumElements * BytesPerElement <= MAX_uint32);

	FRDGBufferDesc Desc;
	Desc.Usage = BUF_Static | BUF_UnorderedAccess | BUF_ShaderResource | BUF_StructuredBuffer | AdditionalFlags;
	Desc.BytesPerElement = BytesPerElement;
	Desc.NumElements = NumElements;

	const TSharedRef<FVoxelRDGExternalBuffer> Result = MakeVoxelShared<FVoxelRDGExternalBuffer>();
	Result->Type = EType::StructuredBuffer;
	Result->Name = Name;
	Result->Format = PF_Unknown;
	Result->BytesPerElement = BytesPerElement;
	Result->NumElements = NumElements;

	FRHIResourceCreateInfo CreateInfo(Name, ResourceArray);
	const FBufferRHIRef Buffer = UE_503_SWITCH(RHICreateStructuredBuffer, RHICmdList.CreateStructuredBuffer)(BytesPerElement, Desc.GetSize(), Desc.Usage, CreateInfo);
	Result->PooledBuffer = new FRDGPooledBuffer(Buffer, Desc, Desc.GetSize(), Name);

	return Result;
}

TSharedRef<FVoxelRDGExternalBuffer> FVoxelRDGExternalBuffer::Create(
	FRHICommandListBase& RHICmdList,
	const TConstArrayView<uint8> Array,
	const EPixelFormat Format,
	const TCHAR* Name,
	const EBufferUsageFlags AdditionalFlags)
{
	const int32 BytesPerElement = GPixelFormats[Format].BlockBytes;
	check(Array.Num() % BytesPerElement == 0);
	const int32 Num = Array.Num() / BytesPerElement;

	FVoxelResourceArrayRef ResourceArray(Array);
	return Create(
		RHICmdList,
		BytesPerElement,
		Num,
		Format,
		Name,
		AdditionalFlags,
		&ResourceArray);
}

FBufferRHIRef FVoxelRDGExternalBuffer::GetBuffer() const
{
	return PooledBuffer->GetRHI();
}

FRHIShaderResourceView* FVoxelRDGExternalBuffer::GetSRV(FRHICommandListBase& RHICmdList) const
{
	FRDGBufferSRVDesc Desc;
#if VOXEL_ENGINE_VERSION < 503
	Desc.BytesPerElement = BytesPerElement;
#endif
	Desc.Format = Format;
	return PooledBuffer->GetOrCreateSRV(UE_503_ONLY(RHICmdList,) Desc);
}

FRHIUnorderedAccessView* FVoxelRDGExternalBuffer::GetUAV(FRHICommandListBase& RHICmdList) const
{
	FRDGBufferUAVDesc Desc;
	Desc.Format = Format;
	return PooledBuffer->GetOrCreateUAV(UE_503_ONLY(RHICmdList,) Desc);
}

void FVoxelRDGExternalBuffer::Resize(
	FRHICommandListBase& RHICmdList,
	const uint32 NewNumElements,
	const bool bCopyData)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelRDGExternalBufferRef NewBuffer;
	if (Type == EType::VertexBuffer)
	{
		NewBuffer = Create(
			RHICmdList,
			BytesPerElement,
			NewNumElements,
			Format,
			Name,
			PooledBuffer->Desc.Usage);
	}
	else
	{
		ensure(Type == EType::StructuredBuffer);
		NewBuffer = CreateStructured(
			RHICmdList,
			BytesPerElement,
			NewNumElements,
			Name,
			PooledBuffer->Desc.Usage);
	}

	if (bCopyData)
	{
		VOXEL_SCOPE_COUNTER("Copy");

		const int64 NumBytes = FMath::Min(GetNumBytes(), NewBuffer->GetNumBytes());
		FRHICommandListExecutor::GetImmediateCommandList().CopyBufferRegion(
			NewBuffer->GetBuffer(),
			0,
			GetBuffer(),
			0,
			NumBytes);
	}

	NumElements = NewNumElements;
	PooledBuffer = NewBuffer->PooledBuffer;
}

void FVoxelRDGExternalBuffer::ResizeIfNeeded(
	FRHICommandListBase& RHICmdList,
	const uint32 NewNumElements,
	const bool bCopyData,
	const float GrowScale)
{
	ensure(GrowScale >= 1);

	if (NewNumElements <= NumElements)
	{
		return;
	}

	Resize(
		RHICmdList,
		FMath::Max<uint32>(NewNumElements, NewNumElements* GrowScale),
		bCopyData);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelRDGBufferBlackboard
{
	TArray<TRefCountPtr<FRDGPooledBuffer>> PooledBuffers;
};
RDG_REGISTER_BLACKBOARD_STRUCT(FVoxelRDGBufferBlackboard);

FVoxelRDGBuffer::FVoxelRDGBuffer(const FVoxelRDGExternalBuffer& ExternalBuffer, FRDGBuilder& GraphBuilder)
	: FVoxelRDGBuffer(ExternalBuffer.Format, GraphBuilder.RegisterExternalBuffer(ExternalBuffer.PooledBuffer.Get()), GraphBuilder)
{
	// Make sure the buffer isn't deleted before the graph execution
	FindOrAddRDGBlackboard<FVoxelRDGBufferBlackboard>(GraphBuilder).PooledBuffers.Add(ExternalBuffer.PooledBuffer.Get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TArray<TSharedPtr<FVoxelShaderStatsScope::FData>> GVoxelShaderStatsScopes;
TFunction<void(uint64 TimeInMicroSeconds, FName Name)> GVoxelShaderStatsCallback;

void FVoxelShaderStatsScope::SetCallback(TFunction<void(uint64 TimeInMicroSeconds, FName Name)> Lambda)
{
	ensure(IsInRenderingThread());
	ensure(!GVoxelShaderStatsCallback || !Lambda);
	GVoxelShaderStatsCallback = MoveTemp(Lambda);
}

FVoxelShaderStatsScope::FVoxelShaderStatsScope(const FName Name)
{
	ensure(IsInRenderingThread());

	if (!GVoxelShaderStatsCallback)
	{
		return;
	}

	VOXEL_FUNCTION_COUNTER();

	Data = MakeVoxelShared<FData>();
	Data->Callback = GVoxelShaderStatsCallback;
	Data->Name = Name;
	Data->TimerQueryPool = RHICreateRenderQueryPool(RQT_AbsoluteTime, 2);
	Data->TimeQueryStart = Data->TimerQueryPool->AllocateQuery();
	Data->TimeQueryEnd = Data->TimerQueryPool->AllocateQuery();

	GVoxelShaderStatsScopes.Add(Data);
}

void FVoxelShaderStatsScope::StartQuery(FRHICommandList& RHICmdList)
{
	if (!Data)
	{
		return;
	}

	RHICmdList.EndRenderQuery(Data->TimeQueryStart.GetQuery());
}

void FVoxelShaderStatsScope::EndQuery(FRHICommandList& RHICmdList)
{
	if (!Data)
	{
		return;
	}

	RHICmdList.EndRenderQuery(Data->TimeQueryEnd.GetQuery());
}

void ProcessVoxelShaderStatsScopes(FRDGBuilder& GraphBuilder)
{
	VOXEL_FUNCTION_COUNTER();

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	for (int32 Index = 0; Index < GVoxelShaderStatsScopes.Num(); Index++)
	{
		const FVoxelShaderStatsScope::FData& Scope = *GVoxelShaderStatsScopes[Index];

		uint64 StartTime = 0;
		uint64 EndTime = 0;
		if (!RHIGetRenderQueryResult(Scope.TimeQueryEnd.GetQuery(), EndTime, false) ||
			!RHIGetRenderQueryResult(Scope.TimeQueryStart.GetQuery(), StartTime, false))
		{
			continue;
		}

		if (ensure(StartTime <= EndTime))
		{
			Scope.Callback(EndTime - StartTime, Scope.Name);
		}

		GVoxelShaderStatsScopes.RemoveAtSwap(Index);
		Index--;
	}
}

VOXEL_RUN_ON_STARTUP_GAME(RegisterProcessVoxelShaderStatsScopes)
{
	FVoxelRenderUtilities::OnPreRender().AddStatic(ProcessVoxelShaderStatsScopes);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelGPUBufferReadback::~FVoxelGPUBufferReadback()
{
	if (IsInRenderingThread())
	{
		return;
	}

	const TSharedRef<TVoxelUniquePtr<FRHIGPUBufferReadback>> ReadbackRef = MakeSharedCopy(MakeUniqueCopy(Readback));

	VOXEL_ENQUEUE_RENDER_COMMAND(FVoxelGPUBufferReadback_DestroyReadback)([ReadbackRef](FRHICommandList& RHICmdList)
	{
		ReadbackRef->Reset();
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void RHIUpdateTexture2D_Safe(
	FRHITexture2D* Texture,
	const uint32 MipIndex,
	const FUpdateTextureRegion2D& UpdateRegion,
	const uint32 SourcePitch,
	const TConstVoxelArrayView<uint8> SourceData)
{
	// FD3D12Texture::UpdateTexture2D doesn't use SrcX/SrcX
	check(UpdateRegion.SrcX == 0);
	check(UpdateRegion.SrcY == 0);

	// See FD3D11DynamicRHI::RHIUpdateTexture2D
	const FPixelFormatInfo& FormatInfo = GPixelFormats[Texture->GetFormat()];
	const size_t UpdateHeightInTiles = FMath::DivideAndRoundUp(UpdateRegion.Height, uint32(FormatInfo.BlockSizeY));
	const size_t SourceDataSize = static_cast<size_t>(SourcePitch) * UpdateHeightInTiles;
	check(SourceData.Num() >= SourceDataSize);

	if (!ensure(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX()) ||
		!ensure(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY()))
	{
		return;
	}

	RHIUpdateTexture2D_Unsafe(
		Texture,
		MipIndex,
		UpdateRegion,
		SourcePitch,
		SourceData.GetData());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FMaterialRenderProxy* FVoxelRenderUtilities::CreateColoredRenderProxy(
	FMeshElementCollector& Collector,
	const FLinearColor& Color,
	const UMaterialInterface* Material)
{
	if (!Material)
	{
		Material = GEngine->ShadedLevelColorationUnlitMaterial;
	}
	if (!ensure(Material))
	{
		return nullptr;
	}

	FColoredMaterialRenderProxy* MaterialProxy = new FColoredMaterialRenderProxy(Material->GetRenderProxy(), Color);
	Collector.RegisterOneFrameMaterialProxy(MaterialProxy);
	return MaterialProxy;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::RenderUtilities
{
	BEGIN_SHADER_PARAMETER_STRUCT(FUploadParameters, )
		RDG_BUFFER_ACCESS(UploadBuffer, ERHIAccess::CopyDest)
	END_SHADER_PARAMETER_STRUCT()
}

void FVoxelRenderUtilities::UpdateBuffer(
	FRHICommandListBase& RHICmdList,
	const FBufferRHIRef& Buffer,
	const void* ArrayData,
	const int64 ArrayTypeSize,
	const int64 ArrayNum,
	const int64 Offset)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Buffer) ||
		!ensure(Buffer->GetSize() >= ArrayTypeSize * (Offset + ArrayNum)))
	{
		return;
	}

	VOXEL_SCOPE_COUNTER_FNAME(Buffer->GetName());

	void* Data = RHICmdList.LockBuffer(Buffer, Offset, ArrayTypeSize * ArrayNum, RLM_WriteOnly);
	if (!ensure(Data))
	{
		return;
	}

	FMemory::Memcpy(Data, ArrayData, ArrayTypeSize * ArrayNum);
	RHICmdList.UnlockBuffer(Buffer);
}

void FVoxelRenderUtilities::UpdateBuffer(
	FRDGBuilder& GraphBuilder,
	const FRDGBufferRef Buffer,
	const void* ArrayData,
	const int64 ArrayTypeSize,
	const int64 ArrayNum,
	const int64 Offset,
	TSharedPtr<FVirtualDestructor> KeepAlive)
{
	using namespace Voxel::RenderUtilities;

	FUploadParameters* UploadParameters = GraphBuilder.AllocParameters<FUploadParameters>();
	UploadParameters->UploadBuffer = Buffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("UploadData"),
		UploadParameters,
		ERDGPassFlags::Copy,
		[=](FRHICommandListImmediate& RHICmdList)
	{
		if (ensure(KeepAlive.IsValid()))
		{
			UpdateBuffer(RHICmdList, Buffer->GetRHI(), ArrayData, ArrayTypeSize, ArrayNum, Offset);
		}
	});
}

DECLARE_GPU_STAT(FVoxelRenderUtilities_CopyBuffer);

BEGIN_SHADER_PARAMETER_STRUCT(FVoxelCopyBufferParameters, )
	RDG_BUFFER_ACCESS(Buffer, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

void FVoxelRenderUtilities::CopyBuffer(FRDGBuilder& GraphBuilder, TSharedPtr<FVoxelGPUBufferReadback>& Readback, FRDGBufferRef Buffer)
{
	check(Buffer);

	if (!Readback)
	{
		Readback = MakeVoxelShareable(new (GVoxelMemory) FVoxelGPUBufferReadback(Buffer->Name));
	}
	else
	{
		ensure(Readback->Readback.IsReady());
	}

	const uint32 NumBytes = Buffer->Desc.GetSize();
	Readback->NumBytes = NumBytes;

	RDG_GPU_STAT_SCOPE(GraphBuilder, FVoxelRenderUtilities_CopyBuffer);
	FVoxelShaderStatsScope* StatScope = GraphBuilder.AllocObject<FVoxelShaderStatsScope>(STATIC_FNAME("Readback"));

	FVoxelCopyBufferParameters* Parameters = GraphBuilder.AllocParameters<FVoxelCopyBufferParameters>();
	Parameters->Buffer = Buffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("EnqueueCopy(%s)", Buffer->Name),
		Parameters,
		ERDGPassFlags::Readback,
		[Readback, Buffer, NumBytes, StatScope](FRHICommandList& RHICmdList)
	{
		StatScope->StartQuery(RHICmdList);
		Readback->Readback.EnqueueCopy(RHICmdList, Buffer->GetRHI(), NumBytes);
		StatScope->EndQuery(RHICmdList);
	});
}

TSharedRef<FVoxelGPUBufferReadback> FVoxelRenderUtilities::CopyBuffer(FRDGBuilder& GraphBuilder, const FRDGBufferRef Buffer)
{
	TSharedPtr<FVoxelGPUBufferReadback> Readback;
	CopyBuffer(GraphBuilder, Readback, Buffer);
	return Readback.ToSharedRef();
}

bool FVoxelRenderUtilities::UpdateTextureRef(
	UTexture2D* TextureObject,
	FRHITexture2D* TextureRHI)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInRenderingThread());

	if (!ensure(TextureObject) ||
		!ensure(TextureRHI))
	{
		return false;
	}

	ensure(TextureObject->GetSizeX() == TextureRHI->GetSizeX());
	ensure(TextureObject->GetSizeY() == TextureRHI->GetSizeY());
	ensure(TextureObject->GetPixelFormat() == TextureRHI->GetFormat());

	FTextureResource* Resource = TextureObject->GetResource();
	if (!ensure(Resource))
	{
		return false;
	}

	FTextureRHIRef OldTextureRHI = Resource->TextureRHI;
	Resource->TextureRHI = TextureRHI;

	struct FHack : FTextureResource
	{
		static FTextureReferenceRHIRef& GetReference(FTextureResource* Resource)
		{
			return static_cast<FHack*>(Resource)->TextureReferenceRHI;
		}
	};

	const FTextureReferenceRHIRef& TextureReferenceRHI = FHack::GetReference(Resource);
	if (ensure(TextureReferenceRHI.IsValid()))
	{
		VOXEL_SCOPE_COUNTER("RHIUpdateTextureReference");
		RHIUpdateTextureReference(TextureReferenceRHI, Resource->TextureRHI);
	}

	VOXEL_SCOPE_COUNTER("SafeRelease");
	OldTextureRHI.SafeRelease();

	return true;
}

void FVoxelRenderUtilities::AsyncCopyTexture(
	const TWeakObjectPtr<UTexture2D> TargetTexture,
	const TSharedRef<const TVoxelArray<uint8>>& Data,
	const FSimpleDelegate& OnComplete)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	if (!ensure(TargetTexture.IsValid()))
	{
		OnComplete.ExecuteIfBound();
		return;
	}

	const int32 SizeX = TargetTexture->GetSizeX();
	const int32 SizeY = TargetTexture->GetSizeY();
	const EPixelFormat Format = TargetTexture->GetPixelFormat();
	if (!ensure(Data->Num() == GPixelFormats[Format].BlockBytes * SizeX * SizeY))
	{
		OnComplete.ExecuteIfBound();
		return;
	}

	if (!GRHISupportsAsyncTextureCreation)
	{
		VOXEL_ENQUEUE_RENDER_COMMAND(AsyncCopyTexture)([=](FRHICommandListImmediate& RHICmdList)
		{
			if (!TargetTexture.IsValid())
			{
				OnComplete.ExecuteIfBound();
				return;
			}

			FTextureResource* Resource = TargetTexture->GetResource();
			if (!ensure(Resource))
			{
				OnComplete.ExecuteIfBound();
				return;
			}

			FRHIResourceCreateInfo CreateInfo(TEXT("AsyncCopyTexture"));

			const TRefCountPtr<FRHITexture2D> UploadTextureRHI = RHICreateTexture(
				FRHITextureCreateDesc::Create2D(TEXT("AsyncCopyTexture"))
				.SetExtent(SizeX, SizeY)
				.SetFormat(Format)
				.SetNumMips(1)
				.SetNumSamples(1)
				.SetFlags(TexCreate_ShaderResource));

			if (!ensure(UploadTextureRHI.IsValid()))
			{
				OnComplete.ExecuteIfBound();
				return;
			}

			uint32 Stride;
			void* LockedData = RHILockTexture2D(UploadTextureRHI, 0, RLM_WriteOnly, Stride, false, false);
			if (ensure(LockedData))
			{
				FMemory::Memcpy(LockedData, Data->GetData(), Data->Num());
			}
			RHIUnlockTexture2D(UploadTextureRHI, 0, false, false);

			Resource->TextureRHI = UploadTextureRHI;

			struct FHack : FTextureResource
			{
				static FTextureReferenceRHIRef& GetReference(FTextureResource* Resource)
				{
					return static_cast<FHack*>(Resource)->TextureReferenceRHI;
				}
			};

			const FTextureReferenceRHIRef& TextureReferenceRHI = FHack::GetReference(Resource);
			if (ensure(TextureReferenceRHI.IsValid()))
			{
				VOXEL_SCOPE_COUNTER("RHIUpdateTextureReference");
				RHIUpdateTextureReference(TextureReferenceRHI, Resource->TextureRHI);
			}

			OnComplete.ExecuteIfBound();
		});

		return;
	}

	Async(EAsyncExecution::ThreadPool, [=]
	{
		VOXEL_SCOPE_COUNTER("RHIAsyncCreateTexture2D");

		TArray<void*> MipData;
		MipData.Add(ConstCast(Data->GetData()));

		FGraphEventRef CompletionEvent;
		const TRefCountPtr_RenderThread<FRHITexture2D> UploadTextureRHI = RHIAsyncCreateTexture2D(
			SizeX,
			SizeY,
			Format,
			1,
			TexCreate_ShaderResource,
			MipData.GetData(),
			1
			UE_503_ONLY(, CompletionEvent));

		if (CompletionEvent.IsValid())
		{
			VOXEL_SCOPE_COUNTER("Wait");
			CompletionEvent->Wait();
		}

		if (!ensure(UploadTextureRHI.IsValid()))
		{
			OnComplete.ExecuteIfBound();
			return;
		}

		VOXEL_ENQUEUE_RENDER_COMMAND(AsyncCopyTexture_Finalize)([=](FRHICommandListImmediate&)
		{
			UpdateTextureRef(TargetTexture.Get(), UploadTextureRHI);
			OnComplete.ExecuteIfBound();
		});
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::RenderUtilities
{
	BEGIN_VOXEL_COMPUTE_SHADER("Voxel/Utilities", BuildIndirectDispatchArgs)
		VOXEL_SHADER_PARAMETER_CST(int32, ThreadGroupSize)
		VOXEL_SHADER_PARAMETER_CST(int32, Multiplier)
		VOXEL_SHADER_PARAMETER_SRV(Buffer<uint>, Num)
		VOXEL_SHADER_PARAMETER_UAV(Buffer<uint>, IndirectDispatchArgs)
	END_VOXEL_SHADER()
}

void FVoxelRenderUtilities::BuildIndirectDispatchArgsFromNum_1D(
	FRDGBuilder& GraphBuilder,
	const int32 ThreadGroupSize,
	const FRDGBufferUAVRef IndirectDispatchArgsUAV,
	const FRDGBufferSRVRef NumSRV,
	const int32 Multiplier)
{
	using namespace Voxel::RenderUtilities;

	BEGIN_VOXEL_SHADER_CALL(BuildIndirectDispatchArgs)
	{
		Parameters.ThreadGroupSize = ThreadGroupSize;
		Parameters.Multiplier = Multiplier;
		Parameters.Num = NumSRV;
		Parameters.IndirectDispatchArgs = IndirectDispatchArgsUAV;

		Execute(FIntVector(1));
	}
	END_VOXEL_SHADER_CALL()
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::RenderUtilities
{
	BEGIN_VOXEL_COMPUTE_SHADER("Voxel/Utilities", ClampNum)
		VOXEL_SHADER_PARAMETER_UAV(Buffer<uint>, Num)
		VOXEL_SHADER_PARAMETER_CST(int32, Min)
		VOXEL_SHADER_PARAMETER_CST(int32, Max)
	END_VOXEL_SHADER()
}

void FVoxelRenderUtilities::ClampNum(
	FRDGBuilder& GraphBuilder,
	const FRDGBufferUAVRef NumUAV,
	const int32 Min,
	const int32 Max)
{
	using namespace Voxel::RenderUtilities;

	BEGIN_VOXEL_SHADER_CALL(ClampNum)
	{
		Parameters.Num = NumUAV;
		Parameters.Min = Min;
		Parameters.Max = Max;

		Execute(FIntVector(1));
	}
	END_VOXEL_SHADER_CALL()
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Voxel::RenderUtilities
{
	BEGIN_VOXEL_COMPUTE_SHADER("Voxel/Utilities", ClearBuffer)
		VOXEL_SHADER_PARAMETER_INDIRECT_ARGS()
		VOXEL_SHADER_PARAMETER_CST(uint32, Value)
		VOXEL_SHADER_PARAMETER_UAV(Buffer<uint>, BufferToClear)
	END_VOXEL_SHADER()
}

void FVoxelRenderUtilities::ClearBuffer(
	FRDGBuilder& GraphBuilder,
	const FRDGBufferUAVRef BufferUAV,
	const FRDGBufferSRVRef NumSRV,
	const uint32 Value,
	const uint32 NumMultiplier)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ClearBuffer %s", BufferUAV->Name);
	ensure(BufferUAV->Desc.Format == PF_R32_UINT);
	using namespace Voxel::RenderUtilities;

	const FVoxelRDGBuffer IndirectDispatchArgs = MakeVoxelRDGBuffer_Indirect(IndirectDispatchArgs);
	BuildIndirectDispatchArgsFromNum_1D(GraphBuilder, 512, IndirectDispatchArgs, NumSRV, NumMultiplier);

	BEGIN_VOXEL_SHADER_INDIRECT_CALL(ClearBuffer)
	{
		Parameters.Value = Value;
		Parameters.BufferToClear = BufferUAV;

		Execute(IndirectDispatchArgs);
	}
	END_VOXEL_SHADER_CALL()
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BEGIN_VOXEL_SHADER_PERMUTATION_DOMAIN(CopyToTextureArray)
{
	class FPixelType : SHADER_PERMUTATION_SPARSE_INT("PIXEL_TYPE_INT", 1, 2);
}
END_VOXEL_SHADER_PERMUTATION_DOMAIN(CopyToTextureArray, FPixelType)

BEGIN_VOXEL_COMPUTE_SHADER("Voxel/Utilities", CopyToTextureArray)
	VOXEL_SHADER_PARAMETER_CST(uint32, SizeX)
	VOXEL_SHADER_PARAMETER_CST(uint32, SliceIndex)
	VOXEL_SHADER_PARAMETER_SRV(Buffer<uint>, SrcBuffer)
	SHADER_PARAMETER_UAV(RWTexture2DArray<uint>, TextureArray)
END_VOXEL_SHADER()

void FVoxelRenderUtilities::CopyToTextureArray(
	FRDGBuilder& GraphBuilder,
	FTexture2DArrayRHIRef TextureArray,
	FUnorderedAccessViewRHIRef TextureArrayUAV,
	FVoxelRDGExternalBufferRef Buffer,
	const int32 SliceIndex)
{
	if (!ensure(TextureArray) ||
		!ensure(TextureArrayUAV) ||
		!ensure(Buffer))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "CopyToTextureArray %s Slice %d", *TextureArray->GetName().ToString(), SliceIndex);

	const int64 NumBytes =
		int64(TextureArray->GetSizeX()) *
		int64(TextureArray->GetSizeY()) *
		GPixelFormats[TextureArray->GetFormat()].BlockBytes;

	if (!ensure(NumBytes == Buffer->GetNumBytes()))
	{
		return;
	}

	BEGIN_VOXEL_SHADER_CALL(CopyToTextureArray)
	{
		if (TextureArray->GetFormat() == PF_G16)
		{
			PermutationDomain.Set<FPixelType>(1);
		}
		else if (
			TextureArray->GetFormat() == PF_B8G8R8A8 ||
			TextureArray->GetFormat() == PF_R8G8B8A8)
		{
			PermutationDomain.Set<FPixelType>(2);
		}
		else
		{
			ensure(false);
		}

		Parameters.SizeX = TextureArray->GetSizeX();
		Parameters.SliceIndex = SliceIndex;
		Parameters.SrcBuffer = FVoxelRDGBuffer(Buffer);
		Parameters.TextureArray = TextureArrayUAV;

		PassFlags |= ERDGPassFlags::NeverCull;

		Execute(FIntVector(
			FVoxelUtilities::DivideCeil<int64>(TextureArray->GetSizeX(), 32),
			FVoxelUtilities::DivideCeil<int64>(TextureArray->GetSizeY(), 32),
			1));
	}
	END_VOXEL_SHADER_CALL()
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_RUN_ON_STARTUP(RegisterOnPreRender, FirstTick, 0)
{
	GEngine->GetPreRenderDelegateEx().AddLambda([](FRDGBuilder& GraphBuilder)
	{
		VOXEL_FUNCTION_COUNTER();

		FVoxelRDGBuilderScope Scope(GraphBuilder);
		FVoxelRenderUtilities::OnPreRender().Broadcast(GraphBuilder);
	});
}

TMulticastDelegate<void(FRDGBuilder&)>& FVoxelRenderUtilities::OnPreRender()
{
	ensure(IsInRenderingThread() || GFrameCounter == 0);

	static TMulticastDelegate<void(FRDGBuilder&)> Delegate;
	return Delegate;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TQueue<TFunction<void(FRDGBuilder&)>, EQueueMode::Mpsc> GVoxelGraphBuilderQueue;

void FlushVoxelGraphBuilderQueue(FRDGBuilder& GraphBuilder)
{
	VOXEL_FUNCTION_COUNTER();

	TFunction<void(FRDGBuilder&)> Lambda;
	while (GVoxelGraphBuilderQueue.Dequeue(Lambda))
	{
		Lambda(GraphBuilder);
	}
}

VOXEL_RUN_ON_STARTUP_GAME(RegisterFlushVoxelGraphBuilderQueue)
{
	FVoxelRenderUtilities::OnPreRender().AddStatic(FlushVoxelGraphBuilderQueue);
}

void FVoxelRenderUtilities::EnqueueGraphBuilderTask(TFunction<void(FRDGBuilder&)> Lambda)
{
	GVoxelGraphBuilderQueue.Enqueue(MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

using FVoxelQueuedReadback = TPair<TSharedPtr<FVoxelGPUBufferReadback>, TFunction<void()>>;

TArray<FVoxelQueuedReadback> GVoxelOnReadbackCompleteList;
TQueue<FVoxelQueuedReadback, EQueueMode::Mpsc> GVoxelOnReadbackCompleteQueue;

void ProcessQueuedVoxelReadbacks()
{
	VOXEL_FUNCTION_COUNTER();

	{
		FVoxelQueuedReadback Readback;
		while (GVoxelOnReadbackCompleteQueue.Dequeue(Readback))
		{
			GVoxelOnReadbackCompleteList.Add(Readback);
		}
	}

	for (int32 Index = 0; Index < GVoxelOnReadbackCompleteList.Num(); Index++)
	{
		const FVoxelQueuedReadback& Readback = GVoxelOnReadbackCompleteList[Index];
		if (!Readback.Key->IsReady())
		{
			continue;
		}

		Readback.Value();
		GVoxelOnReadbackCompleteList.RemoveAtSwap(Index);
		Index--;
	}
}

VOXEL_RUN_ON_STARTUP_GAME(RegisterProcessQueuedVoxelReadbacks)
{
	FCoreDelegates::OnEndFrameRT.AddStatic(ProcessQueuedVoxelReadbacks);
}

void FVoxelRenderUtilities::OnReadbackComplete(const TSharedRef<FVoxelGPUBufferReadback>& Readback, TFunction<void()> OnComplete)
{
	GVoxelOnReadbackCompleteQueue.Enqueue({ Readback, MoveTemp(OnComplete) });
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelGraphBuilderIdBlackboard
{
	int32 Id = 0;
};
RDG_REGISTER_BLACKBOARD_STRUCT(FVoxelGraphBuilderIdBlackboard);

int32 FVoxelRenderUtilities::GetGraphBuilderId()
{
	return GetGraphBuilderId(FVoxelRDGBuilderScope::Get());
}

int32 FVoxelRenderUtilities::GetGraphBuilderId(FRDGBuilder& GraphBuilder)
{
	ensure(IsInRenderingThread());

	if (!GraphBuilder.Blackboard.Get<FVoxelGraphBuilderIdBlackboard>())
	{
		static int32 GlobalCounter = 0;
		GraphBuilder.Blackboard.Create<FVoxelGraphBuilderIdBlackboard>().Id = ++GlobalCounter;
	}

	return GraphBuilder.Blackboard.GetChecked<FVoxelGraphBuilderIdBlackboard>().Id;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelGraphBuilderKeepAliveBlackboard
{
	TSet<const void*> Keys;
	TQueue<TFunction<void()>, EQueueMode::Mpsc> Queue;
};
RDG_REGISTER_BLACKBOARD_STRUCT(FVoxelGraphBuilderKeepAliveBlackboard);

void FVoxelRenderUtilities::KeepAlive(FRDGBuilder& GraphBuilder, const void* Key, TFunction<void()> Lambda)
{
	ensure(IsInRenderingThread());

	FVoxelGraphBuilderKeepAliveBlackboard& Blackboard = FindOrAddRDGBlackboard<FVoxelGraphBuilderKeepAliveBlackboard>(GraphBuilder);

	if (Key && Blackboard.Keys.Contains(Key))
	{
		return;
	}

	Blackboard.Keys.Add(Key);
	Blackboard.Queue.Enqueue(MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TMap<void*, TFunction<void()>> GVoxelKeepAliveThisFrame;

VOXEL_RUN_ON_STARTUP_GAME(RegisterKeepAliveThisFrame)
{
	FCoreDelegates::OnEndFrameRT.AddLambda([]
	{
		VOXEL_SCOPE_COUNTER("KeepAliveThisFrame");

		for (const auto& It : GVoxelKeepAliveThisFrame)
		{
			It.Value();
		}
		GVoxelKeepAliveThisFrame.Reset();
	});
}

void FVoxelRenderUtilities::KeepAliveThisFrame(void* Key, TFunction<void()> Lambda)
{
	ensure(IsInRenderingThread());

	if (GVoxelKeepAliveThisFrame.Contains(Key))
	{
		return;
	}
	GVoxelKeepAliveThisFrame.Add(Key, MoveTemp(Lambda));
}

void FVoxelRenderUtilities::KeepAliveThisFrameAndRelease(const TSharedPtr<FRenderResource>& Resource)
{
	if (!Resource)
	{
		return;
	}

	KeepAliveThisFrame(Resource.Get(), [Resource]
	{
		ensure(Resource.IsUnique());
		Resource->ReleaseResource();
	});
}