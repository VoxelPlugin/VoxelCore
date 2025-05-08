// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelFuture.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "PixelShaderUtils.h"

class FTextureResource;
class FMeshElementCollector;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE void RHIUpdateTexture2D_Unsafe(
	FRHITexture* Texture,
	const uint32 MipIndex,
	const FUpdateTextureRegion2D& UpdateRegion,
	const uint32 SourcePitch,
	const uint8* SourceData)
{
	RHIUpdateTexture2D(
		Texture,
		MipIndex,
		UpdateRegion,
		SourcePitch,
		SourceData);
}

#define RHIUpdateTexture2D UE_DEPRECATED_MACRO(5, "RHIUpdateTexture2D is unsafe, use RHIUpdateTexture2D_Safe instead") RHIUpdateTexture2D

VOXELCORE_API void RHIUpdateTexture2D_Safe(
	FRHITexture* Texture,
	uint32 MipIndex,
	const FUpdateTextureRegion2D& UpdateRegion,
	uint32 SourcePitch,
	TConstVoxelArrayView<uint8> SourceData);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace FVoxelUtilities
{
	VOXELCORE_API FMaterialRenderProxy* CreateColoredMaterialRenderProxy(
		FMeshElementCollector& Collector,
		const FLinearColor& Color,
		const UMaterialInterface* Material = nullptr);

	VOXELCORE_API bool UpdateTextureRef(
		FTextureResource* Resource,
		FRHITexture* TextureRHI);

	VOXELCORE_API FVoxelFuture AsyncCopyTexture(
		UTexture2D* TargetTexture,
		const TSharedRef<const TVoxelArray<uint8>>& Data);

	VOXELCORE_API void ResetPreviousLocalToWorld(const UPrimitiveComponent& Component);

	VOXELCORE_API bool CanUseBarycentricsSemantic(EShaderPlatform ShaderPlatform);
	VOXELCORE_API bool ShouldCompileBarycentricsSemantic(EShaderPlatform ShaderPlatform);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FRDGBufferRef FindBuffer(
		FRDGBuilder& GraphBuilder,
		const FString& Name);

	VOXELCORE_API FRDGTextureRef FindTexture(
		FRDGBuilder& GraphBuilder,
		const FString& Name);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API void UploadBuffer(
		FRDGBuilder& GraphBuilder,
		const FRDGBufferRef& TargetBuffer,
		TConstVoxelArrayView64<uint8> Data,
		const FSharedVoidPtr& KeepAlive);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API TVoxelFuture<TVoxelArray64<uint8>> Readback(
		const FBufferRHIRef& SourceBuffer,
		const TVoxelFuture<int64>& FutureNumBytes = -1);

	template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
	TVoxelFuture<TVoxelArray64<T>> Readback(
		const FBufferRHIRef& SourceBuffer,
		const TVoxelFuture<int32>& FutureNum = -1)
	{
		const TVoxelFuture<int64> FutureNumBytes = FutureNum.Then_AnyThread([](const int32 Num) -> int64
		{
			if (Num == -1)
			{
				return -1;
			}

			return int64(Num) * sizeof(T);
		});

		return Readback(SourceBuffer, FutureNumBytes).Then_AnyThread([](const TVoxelArray64<uint8>& Data)
		{
			VOXEL_SCOPE_COUNTER("Readback Copy");

			if (!ensure(Data.Num() % sizeof(T) == 0))
			{
				return MakeShared<TVoxelArray64<T>>();
			}

			return MakeShared<TVoxelArray64<T>>(MakeVoxelArrayView(Data).ReinterpretAs<T>());
		});
	}

	template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
	TVoxelFuture<T> ReadbackUniform(const FBufferRHIRef& SourceBuffer)
	{
		return Readback(SourceBuffer, sizeof(T)).Then_AnyThread([](const TVoxelArray64<uint8>& Data)
		{
			if (!ensure(Data.Num() == sizeof(T)))
			{
				return FVoxelUtilities::MakeSafe<T>();
			}

			return MakeVoxelArrayView(Data).ReinterpretAs<T>()[0];
		});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API TVoxelFuture<TVoxelArray64<uint8>> Readback(
		FRDGBuilder& GraphBuilder,
		const FRDGBufferRef& SourceBuffer,
		const TVoxelFuture<int64>& FutureNumBytes = -1);

	template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
	TVoxelFuture<TVoxelArray<T>> Readback(
		FRDGBuilder& GraphBuilder,
		const FRDGBufferRef& SourceBuffer,
		const TVoxelFuture<int32>& FutureNum = -1)
	{
		const TVoxelFuture<int64> FutureNumBytes = FutureNum.Then_AnyThread([](const int32 Num) -> int64
		{
			if (Num == -1)
			{
				return -1;
			}

			return int64(Num) * sizeof(T);
		});

		return Readback(GraphBuilder, SourceBuffer, FutureNumBytes).Then_AnyThread([](const TVoxelArray64<uint8>& Data)
		{
			VOXEL_SCOPE_COUNTER("Readback Copy");

			if (!ensure(Data.Num() % sizeof(T) == 0))
			{
				return MakeShared<TVoxelArray<T>>();
			}

			return MakeShared<TVoxelArray<T>>(MakeVoxelArrayView(Data).ReinterpretAs<T>());
		});
	}

	template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
	TVoxelFuture<T> ReadbackUniform(
		FRDGBuilder& GraphBuilder,
		const FRDGBufferRef& SourceBuffer)
	{
		return Readback(GraphBuilder, SourceBuffer, sizeof(T)).Then_AnyThread([](const TVoxelArray64<uint8>& Data)
		{
			if (!ensure(Data.Num() == sizeof(T)))
			{
				return FVoxelUtilities::MakeSafe<T>();
			}

			return MakeVoxelArrayView(Data).ReinterpretAs<T>()[0];
		});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ShaderType, typename ThreadCountType, typename SetupParametersType, typename SetupPermutationDomainType = decltype(nullptr)>
	void AddComputePass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		const ThreadCountType ThreadCount,
		const int32 GroupSize,
		SetupParametersType&& SetupParameters,
		SetupPermutationDomainType&& SetupPermutationDomain = {},
		const ERDGPassFlags PassFlags = ERDGPassFlags::None)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("AddComputePass %s", PassName.GetTCHAR());

		typename ShaderType::FParameters* Parameters = GraphBuilder.AllocParameters<typename ShaderType::FParameters>();
		SetupParameters(*Parameters);

		typename ShaderType::FPermutationDomain PermutationDomain;
		if constexpr (!std::is_null_pointer_v<SetupPermutationDomainType>)
		{
			SetupPermutationDomain(PermutationDomain);
		}

		const TShaderMapRef<ShaderType> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationDomain);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			MoveTemp(PassName),
			PassFlags | ERDGPassFlags::Compute,
			Shader,
			Parameters,
			FComputeShaderUtils::GetGroupCount(ThreadCount, GroupSize));
	}

	template<typename ShaderType, typename ThreadCountType, typename SetupParametersType, typename SetupPermutationDomainType = decltype(nullptr)>
	void AddComputePass(
		FRDGBuilder& GraphBuilder,
		const ThreadCountType ThreadCount,
		const int32 GroupSize,
		SetupParametersType&& SetupParameters,
		SetupPermutationDomainType&& SetupPermutationDomain = {},
		const ERDGPassFlags PassFlags = ERDGPassFlags::None)
	{
		FVoxelUtilities::AddComputePass<ShaderType>(
			GraphBuilder,
			FRDGEventName(ShaderType::GetStaticType().GetName()),
			ThreadCount,
			GroupSize,
			MoveTemp(SetupParameters),
			MoveTemp(SetupPermutationDomain),
			PassFlags);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ShaderType, typename SetupParametersType, typename SetupPermutationDomainType = decltype(nullptr)>
	struct TAddFullscreenPass
	{
	public:
		struct FData
		{
			FRDGBuilder& GraphBuilder;
			const FIntRect Viewport;
			SetupParametersType SetupParameters;
			SetupPermutationDomainType SetupPermutationDomain;

			TOptional<FRDGEventName> PassName;
			const FGlobalShaderMap* ShaderMap = nullptr;
			FRHIBlendState* BlendState = nullptr;
			FRHIRasterizerState* RasterizerState = nullptr;
			FRHIDepthStencilState* DepthStencilState = nullptr;
			uint32 StencilRef = 0;

			FORCEINLINE FData(
				FRDGBuilder& GraphBuilder,
				const FIntRect& Viewport,
				SetupParametersType&& SetupParameters,
				SetupPermutationDomainType&& SetupPermutationDomain)
				: GraphBuilder(GraphBuilder)
				, Viewport(Viewport)
				, SetupParameters(MoveTemp(SetupParameters))
				, SetupPermutationDomain(MoveTemp(SetupPermutationDomain))
			{
			}
			~FData()
			{
				VOXEL_SCOPE_COUNTER_FORMAT("AddFullscreenPass %s", ShaderType::GetStaticType().GetName());

				if (!PassName.IsSet())
				{
					PassName = FRDGEventName(TEXT("%s %dx%d"), ShaderType::GetStaticType().GetName(), Viewport.Size().X, Viewport.Size().Y);
				}
				if (!ShaderMap)
				{
					ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				}
				if (!BlendState)
				{
					BlendState = TStaticBlendState<>::GetRHI();
				}
				if (!RasterizerState)
				{
					RasterizerState = TStaticRasterizerState<>::GetRHI();
				}
				if (!DepthStencilState)
				{
					// Don't write depth, don't depth test
					DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				}

				typename ShaderType::FParameters* Parameters = GraphBuilder.AllocParameters<typename ShaderType::FParameters>();
				SetupParameters(*Parameters);

				typename ShaderType::FPermutationDomain PermutationDomain;
				if constexpr (!std::is_null_pointer_v<SetupPermutationDomainType>)
				{
					SetupPermutationDomain(PermutationDomain);
				}

				const TShaderMapRef<ShaderType> Shader(ShaderMap, PermutationDomain);

				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					ShaderMap,
					MoveTemp(PassName.GetValue()),
					Shader,
					Parameters,
					Viewport,
					BlendState,
					RasterizerState,
					DepthStencilState,
					StencilRef);
			}
		};

		FORCEINLINE TAddFullscreenPass(FData* Data)
			: Data(Data)
		{
		}

		FORCEINLINE TAddFullscreenPass& PassName(FRDGEventName&& PassName)
		{
			Data->PassName = MoveTemp(PassName);
			return *this;
		}
		FORCEINLINE TAddFullscreenPass& ShaderMap(const FGlobalShaderMap* ShaderMap)
		{
			Data->ShaderMap = ShaderMap;
			return *this;
		}
		FORCEINLINE TAddFullscreenPass& BlendState(FRHIBlendState* BlendState)
		{
			Data->BlendState = BlendState;
			return *this;
		}
		FORCEINLINE TAddFullscreenPass& RasterizerState(FRHIRasterizerState* RasterizerState)
		{
			Data->RasterizerState = RasterizerState;
			return *this;
		}
		FORCEINLINE TAddFullscreenPass& DepthStencilState(FRHIDepthStencilState* DepthStencilState)
		{
			Data->DepthStencilState = DepthStencilState;
			return *this;
		}
		FORCEINLINE TAddFullscreenPass& StencilRef(const uint32 StencilRef)
		{
			Data->StencilRef = StencilRef;
			return *this;
		}

	private:
		TUniquePtr<FData> Data;
	};

	template<typename ShaderType, typename SetupParametersType, typename SetupPermutationDomainType = decltype(nullptr)>
	TAddFullscreenPass<ShaderType, SetupParametersType, SetupPermutationDomainType> AddFullscreenPass(
		FRDGBuilder& GraphBuilder,
		const FIntRect& Viewport,
		SetupParametersType&& SetupParameters,
		SetupPermutationDomainType&& SetupPermutationDomain = {})
	{
		return new typename TAddFullscreenPass<ShaderType, SetupParametersType, SetupPermutationDomainType>::FData(
			GraphBuilder,
			Viewport,
			MoveTemp(SetupParameters),
			MoveTemp(SetupPermutationDomain));
	}
};