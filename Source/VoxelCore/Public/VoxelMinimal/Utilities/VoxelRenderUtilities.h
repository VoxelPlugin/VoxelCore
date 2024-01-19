// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/RefCountPtr_RenderThread.h"
#include "Shader.h"
#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Containers/DynamicRHIResourceArray.h"

class UTexture2D;
class FShaderParameterMap;
class FMeshElementCollector;

class VOXELCORE_API FVoxelRDGBuilderScope
{
public:
	explicit FVoxelRDGBuilderScope(FRDGBuilder& Builder)
	{
		check(IsInRenderingThread());
		ensure(!StaticBuilder);
		StaticBuilder = &Builder;
	}
	~FVoxelRDGBuilderScope()
	{
		check(IsInRenderingThread());
		StaticBuilder = nullptr;
	}

	static FRDGBuilder& Get()
	{
		check(IsInRenderingThread());
		check(StaticBuilder);
		return *StaticBuilder;
	}

private:
	static FRDGBuilder* StaticBuilder;
};

template<typename ElementType, uint32 Alignment>
struct TIsContiguousContainer<TResourceArray<ElementType, Alignment>> : TIsContiguousContainer<TArray<ElementType, TMemoryImageAllocator<Alignment>>>
{
};

struct FVoxelResourceArrayRef : public FResourceArrayInterface
{
	const void* const Data;
	const uint32 DataSize;

	template<typename ArrayType>
	FVoxelResourceArrayRef(const ArrayType& Array)
		: Data(GetData(Array))
		, DataSize(GetNum(Array) * sizeof(*::GetData(Array)))
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return DataSize; }

	virtual void Discard() override {}
	virtual bool IsStatic() const override { return true; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override {}
};

class FVoxelRDGExternalBuffer;
using FVoxelRDGExternalBufferRef = TSharedPtr<FVoxelRDGExternalBuffer>;

class VOXELCORE_API FVoxelRDGExternalBuffer
{
public:
	FVoxelRDGExternalBuffer() = default;

	static TSharedRef<FVoxelRDGExternalBuffer> Create(
		const TRefCountPtr<FRDGPooledBuffer>& PooledBuffer,
		EPixelFormat Format,
		const TCHAR* Name);

	static TSharedRef<FVoxelRDGExternalBuffer> Create(
		FRHICommandListBase& RHICmdList,
		int64 BytesPerElement,
		int64 NumElements,
		EPixelFormat Format,
		const TCHAR* Name,
		EBufferUsageFlags AdditionalFlags = BUF_None,
		FResourceArrayInterface* ResourceArray = nullptr);

	static TSharedRef<FVoxelRDGExternalBuffer> CreateStructured(
		FRHICommandListBase& RHICmdList,
		int64 BytesPerElement,
		int64 NumElements,
		const TCHAR* Name,
		EBufferUsageFlags AdditionalFlags = BUF_None,
		FResourceArrayInterface* ResourceArray = nullptr);

public:
	template<typename T>
	static TSharedRef<FVoxelRDGExternalBuffer> CreateTyped(
		FRHICommandListBase& RHICmdList,
		const TConstArrayView<T> Array,
		const EPixelFormat Format,
		const TCHAR* Name)
	{
		FVoxelResourceArrayRef ResourceArray(Array);
		return FVoxelRDGExternalBuffer::Create(
			RHICmdList,
			Array.GetTypeSize(),
			Array.Num(),
			Format,
			Name,
			BUF_None,
			&ResourceArray);
	}

	static TSharedRef<FVoxelRDGExternalBuffer> Create(
		FRHICommandListBase& RHICmdList,
		TConstArrayView<uint8> Array,
		EPixelFormat Format,
		const TCHAR* Name,
		EBufferUsageFlags AdditionalFlags = BUF_None);

public:
	FBufferRHIRef GetBuffer() const;
	FRHIShaderResourceView* GetSRV(FRHICommandListBase& RHICmdList) const;
	FRHIUnorderedAccessView* GetUAV(FRHICommandListBase& RHICmdList) const;

	void Resize(
		FRHICommandListBase& RHICmdList,
		uint32 NewNumElements,
		bool bCopyData);
	void ResizeIfNeeded(
		FRHICommandListBase& RHICmdList,
		uint32 NewNumElements,
		bool bCopyData,
		float GrowScale = 1.25f);

	int64 GetNumBytes() const
	{
		return BytesPerElement * NumElements;
	}
	int64 GetNumElements() const
	{
		return NumElements;
	}
	int64 GetBytesPerElement() const
	{
		return BytesPerElement;
	}

private:
	enum class EType
	{
		None,
		VertexBuffer,
		StructuredBuffer
	};
	EType Type = EType::None;
	const TCHAR* Name = nullptr;

	EPixelFormat Format = PF_Unknown;
	int64 BytesPerElement = 0;
	int64 NumElements = 0;
	TRefCountPtr_RenderThread<FRDGPooledBuffer> PooledBuffer;

	friend class FVoxelRDGBuffer;
};

class VOXELCORE_API FVoxelRDGBuffer
{
public:
	FVoxelRDGBuffer() = default;

	FVoxelRDGBuffer(const EPixelFormat Format, const FRDGBufferRef Buffer, FRDGBuilder& GraphBuilder = FVoxelRDGBuilderScope::Get())
		: GraphBuilder(&GraphBuilder)
		, Format(Format)
		, Buffer(Buffer)
	{
	}
	FVoxelRDGBuffer(const EPixelFormat Format, FRDGBufferDesc Desc, const TCHAR* Name, FRDGBuilder& GraphBuilder = FVoxelRDGBuilderScope::Get())
		: FVoxelRDGBuffer(Format, GraphBuilder.CreateBuffer(Desc, Name), GraphBuilder)
	{
	}
	explicit FVoxelRDGBuffer(FVoxelRDGExternalBufferRef ExternalBuffer, FRDGBuilder& GraphBuilder = FVoxelRDGBuilderScope::Get())
		: FVoxelRDGBuffer(*ExternalBuffer, GraphBuilder)
	{
	}
	explicit FVoxelRDGBuffer(const FVoxelRDGExternalBuffer& ExternalBuffer, FRDGBuilder& GraphBuilder = FVoxelRDGBuilderScope::Get());

	bool IsValid() const
	{
		return GraphBuilder != nullptr;
	}
	operator bool() const
	{
		return IsValid();
	}

	EPixelFormat GetFormat() const
	{
		return Format;
	}

	FRDGBufferRef GetBuffer() const
	{
		return Buffer;
	}
	FRDGBufferUAVRef GetUAV() const
	{
		if (!UAV && Buffer)
		{
			UAV = GraphBuilder->CreateUAV(Buffer, Format);
		}
		return UAV;
	}
	FRDGBufferSRVRef GetSRV() const
	{
		if (!SRV && Buffer)
		{
			SRV = GraphBuilder->CreateSRV(Buffer, Format);
		}
		return SRV;
	}

	uint32 Num() const
	{
		return GetBuffer()->Desc.NumElements;
	}

	operator FRDGBufferRef() const { return GetBuffer(); }
	operator FRDGBufferUAVRef() const { return GetUAV(); }
	operator FRDGBufferSRVRef() const { return GetSRV(); }

private:
	FRDGBuilder* GraphBuilder = nullptr;
	EPixelFormat Format = PF_Unknown;
	FRDGBufferRef Buffer = nullptr;
	mutable FRDGBufferUAVRef UAV = nullptr;
	mutable FRDGBufferSRVRef SRV = nullptr;
};

#define MakeVoxelRDGBuffer(Name, Format, ...) FVoxelRDGBuffer(Format, __VA_ARGS__, TEXT(#Name)); (void)Name; checkVoxelSlow(Name.GetBuffer()->Desc == __VA_ARGS__);
#define MakeVoxelRDGBuffer_Indirect(Name) MakeVoxelRDGBuffer(Name, PF_R32_UINT, FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>())
#define MakeVoxelRDGBuffer_uint32(Name, Size) MakeVoxelRDGBuffer(Name, PF_R32_UINT, FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Size))
#define MakeVoxelRDGBuffer_int32(Name, Size) MakeVoxelRDGBuffer(Name, PF_R32_SINT, FRDGBufferDesc::CreateBufferDesc(sizeof(int32), Size))
#define MakeVoxelRDGBuffer_float(Name, Size) MakeVoxelRDGBuffer(Name, PF_R32_FLOAT, FRDGBufferDesc::CreateBufferDesc(sizeof(float), Size))
#define MakeVoxelRDGBuffer_Structured(Name, BytesPerElement, Size) MakeVoxelRDGBuffer(Name, PF_Unknown, FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, Size))

#define VOXEL_SHADER_PARAMETER_INDIRECT_ARGS() RDG_BUFFER_ACCESS(IndirectDispatchArgsBuffer, ERHIAccess::IndirectArgs)

#if INTELLISENSE_PARSER
template<typename>
struct Buffer;

template<typename>
struct StructuredBuffer;

template<typename>
struct Texture2D;
template<typename>
struct Texture3D;
template<typename>
struct Texture2DArray;

using uint = uint32;

struct SamplerState;

struct float2;
struct float3;
struct float4;

struct int2;
struct int3;
struct int4;

struct uint2;
struct uint3;
struct uint4;

#define VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName) \
	FVoxelDummy_ ## MemberName; \
	struct FVoxelDummyStruct_ ## MemberName \
	{ \
		ShaderType* MemberName; \
	}; \
	typedef zzNextMemberId##MemberName
#else
#define VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)
#endif

#define VOXEL_SHADER_PARAMETER_CST(ShaderType, MemberName) \
	SHADER_PARAMETER(ShaderType, MemberName)

#define VOXEL_SHADER_PARAMETER_SRV(ShaderType, MemberName) \
	SHADER_PARAMETER_RDG_BUFFER_SRV(ShaderType, MemberName) \
	VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)

#define VOXEL_SHADER_PARAMETER_UAV(ShaderType, MemberName) \
	INTERNAL_SHADER_PARAMETER_EXPLICIT(UBMT_RDG_BUFFER_UAV, TShaderResourceParameterTypeInfo<FRDGBufferUAV*>, FRDGBufferUAV*,MemberName,, = nullptr,EShaderPrecisionModifier::Float,TEXT("RW"#ShaderType),false) \
	VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)

#define VOXEL_SHADER_PARAMETER_SAMPLER(ShaderType, MemberName) \
	SHADER_PARAMETER_SAMPLER(ShaderType, MemberName) \
	VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)

#define VOXEL_SHADER_PARAMETER_TEXTURE_SRV(ShaderType, MemberName) \
	SHADER_PARAMETER_RDG_TEXTURE_SRV(ShaderType, MemberName) \
	VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)

#define VOXEL_SHADER_PARAMETER_TEXTURE_UAV(ShaderType, MemberName) \
	INTERNAL_SHADER_PARAMETER_EXPLICIT(UBMT_RDG_TEXTURE_UAV, TShaderResourceParameterTypeInfo<FRDGTextureUAV*>, FRDGTextureUAV*,MemberName,, = nullptr,EShaderPrecisionModifier::Float,TEXT("RW"#ShaderType),false) \
	VOXEL_SHADER_PARAMETER_INTELLISENSE(ShaderType, MemberName)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_SHADER_NAME(Name) Name :: FVoxelShader

template<typename>
struct TVoxelShader_ModifyCompilationEnvironment
{
	static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)
	{
	}
};

template<typename>
struct TVoxelShader_ShouldCompilePermutation
{
	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

template<typename>
struct TVoxelPermutationDomain
{
	using FPermutationDomain = FShaderPermutationNone;
};

#define VOXEL_SHADER_MODIFY_COMPILATION_ENVIRONMENT(Name) \
	namespace Name \
	{ \
		class FVoxelShader; \
	} \
	template<> \
	struct TVoxelShader_ModifyCompilationEnvironment<VOXEL_SHADER_NAME(Name)> \
	{ \
		static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment); \
	}; \
	void TVoxelShader_ModifyCompilationEnvironment<VOXEL_SHADER_NAME(Name)>::ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)

#define BEGIN_VOXEL_SHADER_SHOULD_COMPILE_PERMUTATION(Name) \
	namespace Name \
	{ \
		class FVoxelShader; \
	} \
	template<> \
	struct TVoxelShader_ShouldCompilePermutation<VOXEL_SHADER_NAME(Name)> \
	{ \
		using FPermutationDomain = TVoxelPermutationDomain<VOXEL_SHADER_NAME(Name)>::FPermutationDomain; \
		static bool ShouldCompilePermutation(const FShaderPermutationParameters& Parameters); \
	}; \
	bool TVoxelShader_ShouldCompilePermutation<VOXEL_SHADER_NAME(Name)>::ShouldCompilePermutation(const FShaderPermutationParameters& Parameters) \
	{ \
		using namespace Name; \
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

#define END_VOXEL_SHADER_SHOULD_COMPILE_PERMUTATION() }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define INTERNAL_DECLARE_PERMUTATION(Name, Permutation) Name::Permutation

#define BEGIN_VOXEL_SHADER_PERMUTATION_DOMAIN(Name) \
	namespace Name \
	{ \
		class FVoxelShader; \
	} \
	namespace Name

#define END_VOXEL_SHADER_PERMUTATION_DOMAIN(Name, ...) \
	template<> \
	struct TVoxelPermutationDomain<VOXEL_SHADER_NAME(Name)> \
	{ \
		using FPermutationDomain = TShaderPermutationDomain<VOXEL_FOREACH_ONE_ARG_COMMA(INTERNAL_DECLARE_PERMUTATION, Name, __VA_ARGS__)>; \
	};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelShaderStatsScope
{
public:
	struct FData
	{
		TFunction<void(uint64 TimeInMicroSeconds, FName Name)> Callback;

		FName Name;
		FRenderQueryPoolRHIRef TimerQueryPool;
		FRHIPooledRenderQuery TimeQueryStart;
		FRHIPooledRenderQuery TimeQueryEnd;
	};

	static void SetCallback(TFunction<void(uint64 TimeInMicroSeconds, FName Name)> Lambda);

public:
	explicit FVoxelShaderStatsScope(FName Name);

	void StartQuery(FRHICommandList& RHICmdList);
	void EndQuery(FRHICommandList& RHICmdList);

private:
	TSharedPtr<FData> Data;
};

#define BEGIN_VOXEL_SHADER(FolderPath, Name, InShaderFrequency) \
	namespace Name \
	{ \
		class FVoxelShader : public FGlobalShader \
		{ \
		public: \
			static const TCHAR* GetDebugName() \
			{ \
			    static const FString DebugName = FString("VoxelShader_") + #Name; \
				return *DebugName; \
			} \
			static const TCHAR* GetFunctionName() \
			{ \
				return TEXT("Main"); \
			} \
			static const TCHAR* GetSourceFilename() \
			{ \
			    static const FString Filename = "/Plugin" / FString(FolderPath) / #Name + ".usf"; \
				return *Filename; \
			} \
			static constexpr EShaderFrequency ShaderFrequency = InShaderFrequency; \
			\
		public: \
			using FPermutationDomain = TVoxelPermutationDomain<FVoxelShader>::FPermutationDomain; \
			\
			static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment) \
			{ \
				Environment.SetDefine(TEXT("VOXEL_ENGINE_VERSION"), VOXEL_ENGINE_VERSION); \
				Environment.SetDefine(TEXT("VOXEL_DEBUG"), VOXEL_DEBUG); \
				TVoxelShader_ModifyCompilationEnvironment<FVoxelShader>::ModifyCompilationEnvironment(Parameters, Environment); \
			} \
			static bool ShouldCompilePermutation(const FShaderPermutationParameters& Parameters) \
			{ \
				return TVoxelShader_ShouldCompilePermutation<FVoxelShader>::ShouldCompilePermutation(Parameters); \
			} \
			\
		public: \
			FVoxelShader() = default; \
			FVoxelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
				: FGlobalShader(Initializer) \
			{ \
				BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap, true); \
			} \
			static inline const FShaderParametersMetadata* GetRootParametersMetadata() { return FParameters::FTypeInfo::GetStructMetadata(); } \
			\
		public: \
			DECLARE_GLOBAL_SHADER(FVoxelShader); \
			BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

#define END_VOXEL_SHADER() \
			END_SHADER_PARAMETER_STRUCT() \
		}; \
		using FParameters = FVoxelShader::FParameters; \
		using FPermutationDomain = FVoxelShader::FPermutationDomain; \
		IMPLEMENT_SHADER_TYPE_WITH_DEBUG_NAME(,FVoxelShader, FVoxelShader::GetSourceFilename(), FVoxelShader::GetFunctionName(), FVoxelShader::ShaderFrequency); \
	}

#define BEGIN_VOXEL_COMPUTE_SHADER(FolderPath, Name) BEGIN_VOXEL_SHADER(FolderPath, Name, SF_Compute)

#if HAS_GPU_STATS
#define INTERNAL_VOXEL_SHADER_CALL_GPU_STAT(Name) \
	FRDGGPUStatScopeGuard GpuStatScope(GraphBuilder, STATIC_FNAME(#Name), {}, nullptr, DrawCallCategoryName);
#else
#define INTERNAL_VOXEL_SHADER_CALL_GPU_STAT(Name)
#endif

#define INTERNAL_BEGIN_VOXEL_SHADER_CALL(Name) \
	{ \
		RDG_EVENT_SCOPE(GraphBuilder, #Name); \
		static FDrawCallCategoryName DrawCallCategoryName; \
		INTERNAL_VOXEL_SHADER_CALL_GPU_STAT(Name) \
		FVoxelShaderStatsScope& StatsScope = *GraphBuilder.AllocObject<FVoxelShaderStatsScope>(STATIC_FNAME(#Name)); \
		\
		using namespace Name; \
		FRDGEventName EventName = RDG_EVENT_NAME(#Name); \
		FParameters& Parameters = *GraphBuilder.AllocParameters<VOXEL_SHADER_NAME(Name)::FParameters>(); \
		FPermutationDomain PermutationDomain; \
		ERDGPassFlags PassFlags = ERDGPassFlags::Compute;

#define BEGIN_VOXEL_SHADER_CALL(Name) \
	INTERNAL_BEGIN_VOXEL_SHADER_CALL(Name) \
	const auto Execute = [&](const FIntVector& GroupCount) \
	{ \
		StatsScope.StartQuery(GetImmediateCommandList_ForRenderCommand()); \
		\
		ensure(GroupCount.GetMax() <= MAX_uint16); \
		const TShaderMapRef<FVoxelShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationDomain); \
		FComputeShaderUtils::AddPass( \
			FVoxelRDGBuilderScope::Get(), \
			MoveTemp(EventName), \
			PassFlags, \
			ComputeShader, \
			&Parameters, \
			GroupCount); \
		\
		StatsScope.EndQuery(GetImmediateCommandList_ForRenderCommand()); \
	};

#define BEGIN_VOXEL_SHADER_INDIRECT_CALL(Name) \
	INTERNAL_BEGIN_VOXEL_SHADER_CALL(Name) \
	const auto Execute = [&](FRDGBufferRef IndirectArgsBuffer) \
	{ \
		StatsScope.StartQuery(GetImmediateCommandList_ForRenderCommand()); \
		\
		Parameters.IndirectDispatchArgsBuffer = IndirectArgsBuffer; \
		const TShaderMapRef<FVoxelShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationDomain); \
		FComputeShaderUtils::AddPass( \
			FVoxelRDGBuilderScope::Get(), \
			MoveTemp(EventName), \
			ComputeShader, \
			&Parameters, \
			IndirectArgsBuffer, \
			0); \
		\
		StatsScope.EndQuery(GetImmediateCommandList_ForRenderCommand()); \
	};

#define END_VOXEL_SHADER_CALL() }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelGPUBufferReadback
{
public:
	~FVoxelGPUBufferReadback();

	bool IsReady()
	{
		return Readback.IsReady();
	}

	const void* Lock()
	{
		check(IsReady());
		return Readback.Lock(NumBytes);
	}
	template<typename T>
	TVoxelArrayView<const T> LockView(int64 Num = -1)
	{
		check(NumBytes % sizeof(T) == 0);
		if (Num == -1 || !ensure(Num <= int64(NumBytes / sizeof(T))))
		{
			Num = NumBytes / sizeof(T);
		}

		return TVoxelArrayView<const T>(static_cast<const T*>(Lock()), Num);
	}
	void Unlock()
	{
		Readback.Unlock();
	}

	template<typename T>
	TVoxelArray<T> AsArray(int64 Num = -1)
	{
		VOXEL_FUNCTION_COUNTER();

		check(NumBytes % sizeof(T) == 0);
		if (Num == -1 || !ensure(Num <= int64(NumBytes / sizeof(T))))
		{
			Num = NumBytes / sizeof(T);
		}

		const void* Data = Lock();
		TVoxelArray<T> Array(static_cast<const T*>(Data), Num);
		Unlock();

		return Array;
	}
	template<typename T>
	T As()
	{
		check(NumBytes == sizeof(T));
		T Result;
		const void* Data = Lock();
		FMemory::Memcpy(&Result, Data, sizeof(T));
		Unlock();
		return Result;
	}

private:
	uint32 NumBytes = 0;
	FRHIGPUBufferReadback Readback;

	explicit FVoxelGPUBufferReadback(const FName RequestName)
		: Readback(RequestName)
	{
	}

	friend class FVoxelRenderUtilities;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelPixelFormatInfo
{
	int32 BlockSizeX = 0;
	int32 BlockSizeY = 0;
	int32 BlockSizeZ = 0;
	int32 BlockBytes = 0;
	int32 NumComponents = 0;
};

template<EPixelFormat Format>
FVoxelPixelFormatInfo GetPixelFormat() = delete;

#define Define(Format, BlockSizeX, BlockSizeY, BlockSizeZ, BlockBytes, NumComponents) \
	template<> \
	FORCEINLINE constexpr FVoxelPixelFormatInfo GetPixelFormat<Format>() \
	{ \
		return FVoxelPixelFormatInfo{ BlockSizeX, BlockSizeY, BlockSizeZ, BlockBytes, NumComponents }; \
	}

Define(PF_A32B32G32R32F, 1, 1, 1, 16, 4)
Define(PF_B8G8R8A8, 1, 1, 1, 4, 4)
Define(PF_G8, 1, 1, 1, 1, 1)
Define(PF_G16, 1, 1, 1, 2, 1)
Define(PF_DXT1, 4, 4, 1, 8, 3)
Define(PF_DXT3, 4, 4, 1, 16, 4)
Define(PF_DXT5, 4, 4, 1, 16, 4)
Define(PF_UYVY, 2, 1, 1, 4, 4)
Define(PF_FloatRGB, 1, 1, 1, 4, 3)
Define(PF_FloatRGBA, 1, 1, 1, 8, 4)
Define(PF_DepthStencil, 1, 1, 1, 4, 1)
Define(PF_ShadowDepth, 1, 1, 1, 4, 1)
Define(PF_R32_FLOAT, 1, 1, 1, 4, 1)
Define(PF_G16R16, 1, 1, 1, 4, 2)
Define(PF_G16R16F, 1, 1, 1, 4, 2)
Define(PF_G16R16F_FILTER, 1, 1, 1, 4, 2)
Define(PF_G32R32F, 1, 1, 1, 8, 2)
Define(PF_A2B10G10R10, 1, 1, 1, 4, 4)
Define(PF_A16B16G16R16, 1, 1, 1, 8, 4)
Define(PF_D24, 1, 1, 1, 4, 1)
Define(PF_R16F, 1, 1, 1, 2, 1)
Define(PF_R16F_FILTER, 1, 1, 1, 2, 1)
Define(PF_BC5, 4, 4, 1, 16, 2)
Define(PF_V8U8, 1, 1, 1, 2, 2)
Define(PF_A1, 1, 1, 1, 1, 1)
Define(PF_FloatR11G11B10, 1, 1, 1, 4, 3)
Define(PF_A8, 1, 1, 1, 1, 1)
Define(PF_R32_UINT, 1, 1, 1, 4, 1)
Define(PF_R32_SINT, 1, 1, 1, 4, 1)
Define(PF_PVRTC2, 8, 4, 1, 8, 4)
Define(PF_PVRTC4, 4, 4, 1, 8, 4)
Define(PF_R16_UINT, 1, 1, 1, 2, 1)
Define(PF_R16_SINT, 1, 1, 1, 2, 1)
Define(PF_R16G16B16A16_UINT, 1, 1, 1, 8, 4)
Define(PF_R16G16B16A16_SINT, 1, 1, 1, 8, 4)
Define(PF_R5G6B5_UNORM, 1, 1, 1, 2, 3)
Define(PF_R8G8B8A8, 1, 1, 1, 4, 4)
Define(PF_A8R8G8B8, 1, 1, 1, 4, 4)
Define(PF_BC4, 4, 4, 1, 8, 1)
Define(PF_R8G8, 1, 1, 1, 2, 2)
Define(PF_ATC_RGB, 4, 4, 1, 8, 3)
Define(PF_ATC_RGBA_E, 4, 4, 1, 16, 4)
Define(PF_ATC_RGBA_I, 4, 4, 1, 16, 4)
Define(PF_X24_G8, 1, 1, 1, 1, 1)
Define(PF_ETC1, 4, 4, 1, 8, 3)
Define(PF_ETC2_RGB, 4, 4, 1, 8, 3)
Define(PF_ETC2_RGBA, 4, 4, 1, 16, 4)
Define(PF_R32G32B32A32_UINT, 1, 1, 1, 16, 4)
Define(PF_R16G16_UINT, 1, 1, 1, 4, 4)
Define(PF_ASTC_4x4, 4, 4, 1, 16, 4)
Define(PF_ASTC_6x6, 6, 6, 1, 16, 4)
Define(PF_ASTC_8x8, 8, 8, 1, 16, 4)
Define(PF_ASTC_10x10, 10, 10, 1, 16, 4)
Define(PF_ASTC_12x12, 12, 12, 1, 16, 4)
Define(PF_BC6H, 4, 4, 1, 16, 3)
Define(PF_BC7, 4, 4, 1, 16, 4)
Define(PF_R8_UINT, 1, 1, 1, 1, 1)
Define(PF_L8, 1, 1, 1, 1, 1)
Define(PF_XGXR8, 1, 1, 1, 4, 4)
Define(PF_R8G8B8A8_UINT, 1, 1, 1, 4, 4)
Define(PF_R8G8B8A8_SNORM, 1, 1, 1, 4, 4)
Define(PF_R16G16B16A16_UNORM, 1, 1, 1, 8, 4)
Define(PF_R16G16B16A16_SNORM, 1, 1, 1, 8, 4)
Define(PF_PLATFORM_HDR_0, 0, 0, 0, 0, 0)
Define(PF_PLATFORM_HDR_1, 0, 0, 0, 0, 0)
Define(PF_PLATFORM_HDR_2, 0, 0, 0, 0, 0)
Define(PF_NV12, 1, 1, 1, 1, 1)
Define(PF_R32G32_UINT, 1, 1, 1, 8, 2)
Define(PF_ETC2_R11_EAC, 4, 4, 1, 8, 1)
Define(PF_ETC2_RG11_EAC, 4, 4, 1, 16, 2)
Define(PF_R8, 1, 1, 1, 1, 1)
Define(PF_B5G5R5A1_UNORM, 1, 1, 1, 2, 4)
Define(PF_ASTC_4x4_HDR, 4, 4, 1, 16, 4)
Define(PF_ASTC_6x6_HDR, 6, 6, 1, 16, 4)
Define(PF_ASTC_8x8_HDR, 8, 8, 1, 16, 4)
Define(PF_ASTC_10x10_HDR, 10, 10, 1, 16, 4)
Define(PF_ASTC_12x12_HDR, 12, 12, 1, 16, 4)
Define(PF_G16R16_SNORM, 1, 1, 1, 4, 2)
Define(PF_R8G8_UINT, 1, 1, 1, 2, 2)
Define(PF_R32G32B32_UINT, 1, 1, 1, 12, 3)
Define(PF_R32G32B32_SINT, 1, 1, 1, 12, 3)
Define(PF_R32G32B32F, 1, 1, 1, 12, 3)
Define(PF_R8_SINT, 1, 1, 1, 1, 1)
Define(PF_R64_UINT, 1, 1, 1, 8, 1)

#undef Define

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define VOXEL_ENQUEUE_RENDER_COMMAND(Name) \
	INTELLISENSE_ONLY(struct VOXEL_APPEND_LINE(FDummy) { void Name(); }); \
	[](auto&& Lambda) \
	{ \
		ENQUEUE_RENDER_COMMAND(Name)([Lambda = MoveTemp(Lambda)](FRHICommandListImmediate& RHICmdList) \
		{ \
			VOXEL_SCOPE_COUNTER(#Name); \
			Lambda(RHICmdList); \
		}); \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE void RHIUpdateTexture2D_Unsafe(
	FRHITexture2D* Texture,
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
	FRHITexture2D* Texture,
	uint32 MipIndex,
	const FUpdateTextureRegion2D& UpdateRegion,
	uint32 SourcePitch,
	TConstVoxelArrayView<uint8> SourceData);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
T& FindOrAddRDGBlackboard(FRDGBuilder& GraphBuilder)
{
	checkVoxelSlow(IsInRenderingThread());
	if (!GraphBuilder.Blackboard.Get<T>())
	{
		GraphBuilder.Blackboard.Create<T>();
	}
	return ConstCast(GraphBuilder.Blackboard.GetChecked<T>());
}

class VOXELCORE_API FVoxelRenderUtilities
{
public:
	static FMaterialRenderProxy* CreateColoredRenderProxy(
		FMeshElementCollector& Collector,
		const FLinearColor& Color,
		const UMaterialInterface* Material = nullptr);

public:
	static void UpdateBuffer(
		FRHICommandListBase& RHICmdList,
		const FBufferRHIRef& Buffer,
		const void* ArrayData,
		int64 ArrayTypeSize,
		int64 ArrayNum,
		int64 Offset);

	static void UpdateBuffer(
		FRDGBuilder& GraphBuilder,
		FRDGBufferRef Buffer,
		const void* ArrayData,
		int64 ArrayTypeSize,
		int64 ArrayNum,
		int64 Offset,
		TSharedPtr<FVirtualDestructor> KeepAlive);

	template<typename ArrayType>
	static void UpdateBuffer(
		FRDGBuilder& GraphBuilder,
		const FBufferRHIRef& Buffer,
		const ArrayType& Array,
		const int64 Offset = 0)
	{
		FVoxelRenderUtilities::UpdateBuffer(
			GraphBuilder.RHICmdList,
			Buffer,
			Array.GetData(),
			Array.GetTypeSize(),
			Array.Num(),
			Offset);
	}

	template<typename ArrayType, typename KeepAliveType>
	static void UpdateBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef Buffer, const TSharedRef<KeepAliveType>& KeepAlive, const ArrayType& Array, int64 Offset = 0)
	{
		struct FKeepAlive : FVirtualDestructor
		{
			TSharedPtr<KeepAliveType> KeepAlive;
		};
		const TSharedRef<FKeepAlive> KeepAlivePtr = MakeVoxelShared<FKeepAlive>();
		KeepAlivePtr->KeepAlive = KeepAlive;
		FVoxelRenderUtilities::UpdateBuffer(GraphBuilder, Buffer, Array.GetData(), Array.GetTypeSize(), Array.Num(), Offset, KeepAlivePtr);
	}

	template<typename ArrayType>
	static void UpdateBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef Buffer, ArrayType&& InArray, int64 Offset = 0)
	{
		struct FArrayWrapper : FVirtualDestructor
		{
			ArrayType Array;
		};
		const TSharedRef<FArrayWrapper> ArrayWrapper = MakeVoxelShared<FArrayWrapper>();
		ArrayWrapper->Array = MoveTemp(InArray);
		FVoxelRenderUtilities::UpdateBuffer(
			GraphBuilder,
			Buffer,
			ArrayWrapper->Array.GetData(),
			ArrayWrapper->Array.GetTypeSize(),
			ArrayWrapper->Array.Num(),
			Offset,
			ArrayWrapper);
	}

	static void CopyBuffer(FRDGBuilder& GraphBuilder, TSharedPtr<FVoxelGPUBufferReadback>& Readback, FRDGBufferRef Buffer);
	static TSharedRef<FVoxelGPUBufferReadback> CopyBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef Buffer);

	static bool UpdateTextureRef(
		UTexture2D* TextureObject,
		FRHITexture2D* TextureRHI);

	static void AsyncCopyTexture(
		TWeakObjectPtr<UTexture2D> TargetTexture,
		const TSharedRef<const TVoxelArray<uint8>>& Data,
		const FSimpleDelegate& OnComplete);

public:
	static void BuildIndirectDispatchArgsFromNum_1D(
		FRDGBuilder& GraphBuilder,
		int32 ThreadGroupSize,
		FRDGBufferUAVRef IndirectDispatchArgsUAV,
		FRDGBufferSRVRef NumSRV,
		int32 Multiplier = 1);

	static void ClampNum(
		FRDGBuilder& GraphBuilder,
		FRDGBufferUAVRef NumUAV,
		int32 Min,
		int32 Max);

	static void ClearBuffer(
		FRDGBuilder& GraphBuilder,
		FRDGBufferUAVRef BufferUAV,
		FRDGBufferSRVRef NumSRV,
		uint32 Value,
		uint32 NumMultiplier = 1);

	static void CopyToTextureArray(
		FRDGBuilder& GraphBuilder,
		FTexture2DArrayRHIRef TextureArray,
		FUnorderedAccessViewRHIRef TextureArrayUAV,
		FVoxelRDGExternalBufferRef Buffer,
		int32 SliceIndex);

public:
	static TMulticastDelegate<void(FRDGBuilder&)>& OnPreRender();
	static void EnqueueGraphBuilderTask(TFunction<void(FRDGBuilder&)> Lambda);
	static void OnReadbackComplete(const TSharedRef<FVoxelGPUBufferReadback>& Readback, TFunction<void()> OnComplete);

	static int32 GetGraphBuilderId();
	static int32 GetGraphBuilderId(FRDGBuilder& GraphBuilder);

public:
	static void KeepAlive(FRDGBuilder& GraphBuilder, const void* Key, TFunction<void()> Lambda);

	template<typename T>
	static void KeepAlive(FRDGBuilder& GraphBuilder, const TSharedPtr<T>& Data)
	{
		FVoxelRenderUtilities::KeepAlive(GraphBuilder, Data.Get(), [Data]
		{
			(void)Data;
		});
	}
	template<typename T>
	static void KeepAlive(FRDGBuilder& GraphBuilder, const TSharedRef<T>& Data)
	{
		FVoxelRenderUtilities::KeepAlive(GraphBuilder, &Data.Get(), [Data]
		{
			(void)Data;
		});
	}

public:
	static void KeepAliveThisFrame(void* Key, TFunction<void()> Lambda);
	static void KeepAliveThisFrameAndRelease(const TSharedPtr<FRenderResource>& Resource);
};