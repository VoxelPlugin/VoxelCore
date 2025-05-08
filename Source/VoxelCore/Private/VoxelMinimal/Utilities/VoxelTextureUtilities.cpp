// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "TextureCompiler.h"
#include "TextureResource.h"
#include "Engine/Texture2DArray.h"

THIRD_PARTY_INCLUDES_START
#include "png.h"
#include "zlib.h"
THIRD_PARTY_INCLUDES_END

UTexture2D* FVoxelTextureUtilities::GetDefaultTexture2D()
{
	// Loaded by UVoxelTextureUtilitiesHelper
	UTexture2D* Texture = FindObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	ensure(Texture);
	return Texture;
}

UTexture2DArray* FVoxelTextureUtilities::GetDefaultTexture2DArray()
{
	// Loaded by UVoxelTextureUtilitiesHelper
	UTexture2DArray* Texture = FindObject<UTexture2DArray>(nullptr, TEXT("/Voxel/Default/DefaultTextureArray.DefaultTextureArray"));
	ensure(Texture);
	return Texture;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

EMaterialSamplerType FVoxelTextureUtilities::GetSamplerType(const UTexture& Texture)
{
	switch (Texture.CompressionSettings)
	{
	case TC_Normalmap: return SAMPLERTYPE_Normal;
	case TC_Grayscale: return Texture.SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
	case TC_Masks: return SAMPLERTYPE_Masks;
	case TC_Alpha: return SAMPLERTYPE_Alpha;
	default: return Texture.SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
	}
}

FString FVoxelTextureUtilities::GetSamplerFunction(const EMaterialSamplerType SamplerType)
{
	switch (SamplerType)
	{
	default: ensure(false);
	case SAMPLERTYPE_External:
	{
		return "ProcessMaterialExternalTextureLookup";
	}
	case SAMPLERTYPE_Color:
	{
		return "ProcessMaterialColorTextureLookup";
	}
	case SAMPLERTYPE_VirtualColor:
	{
		return "ProcessMaterialVirtualColorTextureLookup";
	}
	case SAMPLERTYPE_LinearColor:
	case SAMPLERTYPE_VirtualLinearColor:
	{
		return "ProcessMaterialLinearColorTextureLookup";
	}
	case SAMPLERTYPE_Alpha:
	case SAMPLERTYPE_VirtualAlpha:
	case SAMPLERTYPE_DistanceFieldFont:
	{
		return "ProcessMaterialAlphaTextureLookup";
	}
	case SAMPLERTYPE_Grayscale:
	case SAMPLERTYPE_VirtualGrayscale:
	{
		return "ProcessMaterialGreyscaleTextureLookup";
	}
	case SAMPLERTYPE_LinearGrayscale:
	case SAMPLERTYPE_VirtualLinearGrayscale:
	{
		return "ProcessMaterialLinearGreyscaleTextureLookup";
	}
	case SAMPLERTYPE_Normal:
	case SAMPLERTYPE_VirtualNormal:
	{
		return "UnpackNormalMap";
	}
	case SAMPLERTYPE_Masks:
	case SAMPLERTYPE_VirtualMasks:
	case SAMPLERTYPE_Data:
	{
		return "";
	}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UTexture2D* FVoxelTextureUtilities::CreateTexture2D(
	const FName DebugName,
	const int32 SizeX,
	const int32 SizeY,
	const bool bSRGB,
	const TextureFilter Filter,
	const EPixelFormat PixelFormat,
	TFunction<void(TVoxelArrayView64<uint8> Data)> InitializeMip0,
	UTexture2D* ExistingTexture)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(SizeX > 0) ||
		!ensure(SizeY > 0) ||
		!ensure(SizeX % GPixelFormats[PixelFormat].BlockSizeX == 0) ||
		!ensure(SizeY % GPixelFormats[PixelFormat].BlockSizeY == 0))
	{
		return nullptr;
	}

	const int32 NumBlocksX = SizeX / GPixelFormats[PixelFormat].BlockSizeX;
	const int32 NumBlocksY = SizeY / GPixelFormats[PixelFormat].BlockSizeY;

	const int64 NumBytes = int64(NumBlocksX) * int64(NumBlocksY) * int64(GPixelFormats[PixelFormat].BlockBytes);
	if (!ensure(NumBytes <= MAX_uint32))
	{
		return nullptr;
	}

	UTexture2D* Texture = ExistingTexture;
	if (!Texture)
	{
		const FName ObjectName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), *FString::Printf(TEXT("Texture2D_%s"), *DebugName.ToString()));
		Texture = NewObject<UTexture2D>(GetTransientPackage(), ObjectName);
	}

	Texture->SRGB = bSRGB;
	Texture->Filter = Filter;
	// Not sure if needed
	Texture->NeverStream = true;

	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = SizeX;
	PlatformData->SizeY = SizeY;
	PlatformData->PixelFormat = PixelFormat;

	FTexture2DMipMap* Mip = new FTexture2DMipMap();
	PlatformData->Mips.Add(Mip);
	Mip->SizeX = SizeX;
	Mip->SizeY = SizeY;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	{
		VOXEL_SCOPE_COUNTER("AllocateResource");

		void* Data = Mip->BulkData.Realloc(NumBytes);
		if (ensure(Data))
		{
			const TVoxelArrayView64<uint8> DataView(static_cast<uint8*>(Data), NumBytes);
			if (InitializeMip0)
			{
				InitializeMip0(DataView);
			}
			else
			{
				FVoxelUtilities::Memzero(DataView);
			}
		}
	}
	Mip->BulkData.Unlock();

	Texture->SetPlatformData(PlatformData);

	{
		VOXEL_SCOPE_COUNTER("UpdateResource");
		Texture->UpdateResource();
	}

	// We don't need to keep bulk data around
	Mip->BulkData.RemoveBulkData();

	return Texture;
}

void FVoxelTextureUtilities::RemoveBulkData(UTexture2D* Texture)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!ensure(Texture))
	{
		return;
	}

	// Make sure texture is streamed in before clearing bulk data
	Voxel::RenderTask([WeakTexture = MakeWeakObjectPtr(Texture)]
	{
		Voxel::GameTask([=]
		{
			UTexture2D* LocalTexture = WeakTexture.Get();
			if (!ensure(LocalTexture))
			{
				return;
			}

			FTexturePlatformData* PlatformData = LocalTexture->GetPlatformData();
			if (!ensure(PlatformData) ||
				!ensure(PlatformData->Mips.Num() == 1))
			{
				return;
			}

			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			Mip.BulkData.RemoveBulkData();
		});
	});
}

UTexture2DArray* FVoxelTextureUtilities::CreateTextureArray(
	const FName DebugName,
	const int32 SizeX,
	const int32 SizeY,
	const int32 SizeZ,
	const bool bSRGB,
	const TextureFilter Filter,
	const EPixelFormat PixelFormat,
	const int32 NumMips,
	TFunction<void(TVoxelArrayView<uint8> Data, int32 MipIndex)> InitializeMip,
	UTexture2DArray* ExistingTexture)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(SizeX > 0) ||
		!ensure(SizeY > 0) ||
		!ensure(SizeZ > 0) ||
		!ensure(SizeX % GPixelFormats[PixelFormat].BlockSizeX == 0) ||
		!ensure(SizeY % GPixelFormats[PixelFormat].BlockSizeY == 0) ||
		!ensure(SizeZ % GPixelFormats[PixelFormat].BlockSizeZ == 0))
	{
		return nullptr;
	}

	UTexture2DArray* TextureArray = ExistingTexture;
	if (!TextureArray)
	{
		const FName ObjectName = MakeUniqueObjectName(GetTransientPackage(), UTexture2DArray::StaticClass(), *FString::Printf(TEXT("Texture2DArray_%s"), *DebugName.ToString()));
		TextureArray = NewObject<UTexture2DArray>(GetTransientPackage(), ObjectName);
	}

	TextureArray->SRGB = bSRGB;
	TextureArray->Filter = Filter;

	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = SizeX;
	PlatformData->SizeY = SizeY;
	PlatformData->SetNumSlices(SizeZ);
	PlatformData->PixelFormat = PixelFormat;

	if (const FTexturePlatformData* ExistingPlatformData = TextureArray->GetPlatformData())
	{
		delete ExistingPlatformData;
		TextureArray->SetPlatformData(nullptr);
	}

	check(!TextureArray->GetPlatformData());
	TextureArray->SetPlatformData(PlatformData);

	ensure(NumMips >= 1);
	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		ensure(SizeX % (1 << MipIndex) == 0);
		ensure(SizeY % (1 << MipIndex) == 0);

		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Mip->SizeX = SizeX >> MipIndex;
		Mip->SizeY = SizeY >> MipIndex;
		Mip->SizeZ = SizeZ;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		{
			VOXEL_SCOPE_COUNTER("AllocateResource");

			const int32 NumBlocksX = Mip->SizeX / GPixelFormats[PixelFormat].BlockSizeX;
			const int32 NumBlocksY = Mip->SizeY / GPixelFormats[PixelFormat].BlockSizeY;
			const int32 NumBlocksZ = Mip->SizeZ / GPixelFormats[PixelFormat].BlockSizeZ;

			const int64 NumBytes = int64(NumBlocksX) * int64(NumBlocksY) * int64(NumBlocksZ) * int64(GPixelFormats[PixelFormat].BlockBytes);
			if (!ensure(NumBytes < MAX_uint32))
			{
				return nullptr;
			}

			void* Data = Mip->BulkData.Realloc(NumBytes);
			if (ensure(Data))
			{
				const TVoxelArrayView<uint8> DataView(static_cast<uint8*>(Data), NumBytes);
				if (InitializeMip)
				{
					InitializeMip(DataView, MipIndex);
				}
				else
				{
					FVoxelUtilities::Memzero(DataView);
				}
			}
		}
		Mip->BulkData.Unlock();

		PlatformData->Mips.Add(Mip);
	}

	{
		VOXEL_SCOPE_COUNTER("UpdateResource");
		TextureArray->UpdateResource();
	}

	for (FTexture2DMipMap& Mip : PlatformData->Mips)
	{
		// We don't need to keep bulk data around
		Mip.BulkData.RemoveBulkData();
	}

	return TextureArray;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelArray64<uint8> FVoxelTextureUtilities::CompressPng_RGB(
	const TConstVoxelArrayView64<FVoxelColor3> ColorData,
	const int32 Width,
	const int32 Height)
{
	VOXEL_FUNCTION_COUNTER();

	check(ColorData.Num() == int64(Width) * int64(Height));

	TVoxelArray64<uint8> CompressedData;

	png_structp PngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	check(PngStruct);

	png_infop PngInfo = png_create_info_struct(PngStruct);
	check(PngInfo);

	png_bytep* RowPointers = static_cast<png_bytep*>(png_malloc(PngStruct, Height * sizeof(png_bytep)));
	ON_SCOPE_EXIT
	{
		png_free(PngStruct, RowPointers);
		png_destroy_write_struct(&PngStruct, &PngInfo);
	};

	png_set_compression_level(PngStruct, Z_BEST_SPEED);
	png_set_IHDR(PngStruct, PngInfo, Width, Height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_write_fn(
		PngStruct,
		&CompressedData,
		[](const png_structp PngStruct, const png_bytep Data, const png_size_t Length)
		{
			TVoxelArray64<uint8>* CompressedDataPtr = static_cast<TVoxelArray64<uint8>*>(png_get_io_ptr(PngStruct));
			CompressedDataPtr->Append(Data, Length);
		},
		nullptr);

	for (int64 Index = 0; Index < Height; Index++)
	{
		RowPointers[Index] = ConstCast(reinterpret_cast<const uint8*>(&ColorData[Index * Width]));
	}
	png_set_rows(PngStruct, PngInfo, RowPointers);
	png_write_png(PngStruct, PngInfo, PNG_TRANSFORM_IDENTITY, nullptr);

	return CompressedData;
}

TVoxelArray64<uint8> FVoxelTextureUtilities::CompressPng_Grayscale(
	const TConstVoxelArrayView64<uint16> GrayscaleData,
	const int32 Width,
	const int32 Height)
{
	VOXEL_FUNCTION_COUNTER();

	check(GrayscaleData.Num() == Width * Height);

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ensure(ImageWrapper))
	{
		return {};
	}

	if (!ensure(ImageWrapper->SetRaw(
		GrayscaleData.GetData(),
		GrayscaleData.Num() * sizeof(uint16),
		Width,
		Height,
		ERGBFormat::Gray,
		16)))
	{
		return {};
	}

	return TVoxelArray64<uint8>(ImageWrapper->GetCompressed());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelTextureUtilities::UncompressPng_RGB(
	const TConstVoxelArrayView64<uint8> CompressedData,
	TVoxelArray64<FVoxelColor3>& OutColorData,
	int32& OutWidth,
	int32& OutHeight)
{
	return Uncompress_RGB(
		CompressedData,
		OutColorData,
		OutWidth,
		OutHeight);
}

bool FVoxelTextureUtilities::Uncompress_RGB(
	const TConstVoxelArrayView64<uint8> CompressedData,
	TVoxelArray64<FVoxelColor3>& OutColorData,
	int32& OutWidth,
	int32& OutHeight)
{
	VOXEL_FUNCTION_COUNTER();

	OutColorData.Reset();
	OutWidth = 0;
	OutHeight = 0;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedData.GetData(), CompressedData.Num());
	if (!ensure(ImageFormat != EImageFormat::Invalid))
	{
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ensure(ImageWrapper))
	{
		return false;
	}

	{
		VOXEL_SCOPE_COUNTER("SetCompressed");

		if (!ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
		{
			ensure(false);
			return false;
		}
	}

	TArray64<uint8> RawData;
	{
		VOXEL_SCOPE_COUNTER("GetRaw");

		if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData))
		{
			ensure(false);
			return false;
		}
	}

	OutWidth = ImageWrapper->GetWidth();
	OutHeight = ImageWrapper->GetHeight();

	const int64 Num = int64(OutWidth) * int64(OutHeight);
	OutColorData.Reserve(Num);
	OutColorData.SetNumUninitialized(Num);
	for (int64 Index = 0; Index < Num; Index++)
	{
		OutColorData[Index] = FVoxelColor3(
			RawData[4 * Index + 0],
			RawData[4 * Index + 1],
			RawData[4 * Index + 2]);
		ensureVoxelSlow(RawData[4 * Index + 3] == 255);
	}

	return true;
}

bool FVoxelTextureUtilities::UncompressPng_Grayscale(
	const TConstVoxelArrayView64<uint8> CompressedData,
	TVoxelArray64<uint16>& OutGrayscaleData,
	int32& OutWidth,
	int32& OutHeight)
{
	VOXEL_FUNCTION_COUNTER();

	OutGrayscaleData.Reset();
	OutWidth = 0;
	OutHeight = 0;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

	const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(CompressedData.GetData(), CompressedData.Num());
	if (!ensure(ImageFormat != EImageFormat::Invalid))
	{
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ensure(ImageWrapper))
	{
		return false;
	}

	{
		VOXEL_SCOPE_COUNTER("SetCompressed");

		if (!ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
		{
			ensure(false);
			return false;
		}
	}

	TArray64<uint8> RawData;
	{
		VOXEL_SCOPE_COUNTER("GetRaw");

		if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawData))
		{
			ensure(false);
			return false;
		}
	}

	if (!ensure(ImageWrapper->GetBitDepth() == 16))
	{
		return false;
	}

	OutWidth = ImageWrapper->GetWidth();
	OutHeight = ImageWrapper->GetHeight();

	const int64 Num = OutWidth * OutHeight;
	if (!ensure(RawData.Num() == 2 * Num))
	{
		return false;
	}

	OutGrayscaleData.Reserve(Num);
	OutGrayscaleData.SetNumUninitialized(Num);

	FMemory::Memcpy(OutGrayscaleData.GetData(), RawData.GetData(), RawData.Num());

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTextureUtilities::FullyLoadTextures(const TArray<UTexture*>& Textures)
{
#if WITH_EDITOR
	VOXEL_FUNCTION_COUNTER();

	FTextureCompilingManager::Get().FinishCompilation(Textures);
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool FVoxelTextureUtilities::ExtractTextureChannel(
	const UTexture2D& Texture,
	const EVoxelTextureChannel Channel,
	int32& OutSizeX,
	int32& OutSizeY,
	TVoxelArray<float>& OutValues)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	const FTextureSource& Source = Texture.Source;
	if (!ensureVoxelSlow(Source.IsValid()))
	{
		return false;
	}

	// Flush async tasks to ensure FTextureSource::GetMipData is not called while we extract data
	ConstCast(Texture).BlockOnAnyAsyncBuild();

	OutSizeX = Source.GetSizeX();
	OutSizeY = Source.GetSizeY();
	FVoxelUtilities::SetNumFast(OutValues, OutSizeX * OutSizeY);

	const int32 NumPixels = OutSizeX * OutSizeY;

	const TConstVoxelArrayView64<uint8> SourceByteData(
		ConstCast(Source).LockMipReadOnly(0, 0, 0),
		Source.CalcMipSize(0, 0, 0));
	ON_SCOPE_EXIT
	{
		ConstCast(Source).UnlockMip(0, 0, 0);
	};

	switch (Source.GetFormat())
	{
	default:
	{
		ensureVoxelSlow(false);
		VOXEL_MESSAGE(Error, "Unsupported format: {0}", UEnum::GetValueAsString(Source.GetFormat()));
		return false;
	}
	case TSF_G8:
	{
		if (!ensure(SourceByteData.Num() == NumPixels))
		{
			return false;
		}

		for (int32 Index = 0; Index < NumPixels; Index++)
		{
			OutValues[Index] = SourceByteData[Index] / float(MAX_uint8);
		}

		return true;
	}
	case TSF_BGRA8:
	case TSF_BGRE8:
	{
		const TConstVoxelArrayView64<FColor> SourceData = SourceByteData.ReinterpretAs<FColor>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		switch (Channel)
		{
		default:
		{
			ensure(false);
			return false;
		}
		case EVoxelTextureChannel::R:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].R / float(MAX_uint8);
			}
		}
		break;
		case EVoxelTextureChannel::G:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].G / float(MAX_uint8);
			}
		}
		break;
		case EVoxelTextureChannel::B:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].B / float(MAX_uint8);
			}
		}
		break;
		case EVoxelTextureChannel::A:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].A / float(MAX_uint8);
			}
		}
		break;
		}

		return true;
	}
	case TSF_RGBA16:
	{
		struct FColor16
		{
			uint16 R;
			uint16 G;
			uint16 B;
			uint16 A;
		};

		const TConstVoxelArrayView64<FColor16> SourceData = SourceByteData.ReinterpretAs<FColor16>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		switch (Channel)
		{
		default:
		{
			ensure(false);
			return false;
		}
		case EVoxelTextureChannel::R:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].R / float(MAX_uint16);
			}
		}
		break;
		case EVoxelTextureChannel::G:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].G / float(MAX_uint16);
			}
		}
		break;
		case EVoxelTextureChannel::B:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].B / float(MAX_uint16);
			}
		}
		break;
		case EVoxelTextureChannel::A:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].A / float(MAX_uint16);
			}
		}
		break;
		}

		return true;
	}
	case TSF_RGBA16F:
	{
		struct FColor16F
		{
			FFloat16 R;
			FFloat16 G;
			FFloat16 B;
			FFloat16 A;
		};

		const TConstVoxelArrayView64<FColor16F> SourceData = SourceByteData.ReinterpretAs<FColor16F>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		switch (Channel)
		{
		default:
		{
			ensure(false);
			return false;
		}
		case EVoxelTextureChannel::R:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].R;
			}
		}
		break;
		case EVoxelTextureChannel::G:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].G;
			}
		}
		break;
		case EVoxelTextureChannel::B:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].B;
			}
		}
		break;
		case EVoxelTextureChannel::A:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].A;
			}
		}
		break;
		}

		return true;
	}
	case TSF_G16:
	{
		const TConstVoxelArrayView64<uint16> SourceData = SourceByteData.ReinterpretAs<uint16>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		for (int32 Index = 0; Index < NumPixels; Index++)
		{
			OutValues[Index] = SourceData[Index] / float(MAX_uint16);
		}

		return true;
	}
	case TSF_RGBA32F:
	{
		const TConstVoxelArrayView64<FLinearColor> SourceData = SourceByteData.ReinterpretAs<FLinearColor>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		switch (Channel)
		{
		default:
		{
			ensure(false);
			return false;
		}
		case EVoxelTextureChannel::R:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].R;
			}
		}
		break;
		case EVoxelTextureChannel::G:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].G;
			}
		}
		break;
		case EVoxelTextureChannel::B:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].B;
			}
		}
		break;
		case EVoxelTextureChannel::A:
		{
			for (int32 Index = 0; Index < NumPixels; Index++)
			{
				OutValues[Index] = SourceData[Index].A;
			}
		}
		break;
		}

		return true;
	}
	case TSF_R16F:
	{
		const TConstVoxelArrayView64<FFloat16> SourceData = SourceByteData.ReinterpretAs<FFloat16>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		for (int32 Index = 0; Index < NumPixels; Index++)
		{
			OutValues[Index] = SourceData[Index];
		}

		return true;
	}
	case TSF_R32F:
	{
		const TConstVoxelArrayView64<float> SourceData = SourceByteData.ReinterpretAs<float>();
		if (!ensure(SourceData.Num() == NumPixels))
		{
			return false;
		}

		FVoxelUtilities::Memcpy(OutValues, SourceData);
		return true;
	}
	}
}
#endif