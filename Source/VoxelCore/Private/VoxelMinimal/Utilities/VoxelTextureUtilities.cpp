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
	// Loaded by UMaterialExpressionSampleVoxelTextureParameter
	UTexture2D* Texture = FindObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	ensure(Texture);
	return Texture;
}

UTexture2DArray* FVoxelTextureUtilities::GetDefaultTexture2DArray()
{
	// Loaded by UMaterialExpressionSampleVoxelTextureParameter
	UTexture2DArray* Texture = FindObject<UTexture2DArray>(nullptr, TEXT("/Voxel/Default/DefaultTextureArray.DefaultTextureArray"));
	ensure(Texture);
	return Texture;
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
	TFunction<void(TVoxelArrayView<uint8> Data)> InitializeMip0,
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
	if (!ensure(NumBytes < MAX_int32))
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
			const TVoxelArrayView<uint8> DataView(static_cast<uint8*>(Data), NumBytes);
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

	if (const FTexturePlatformData* ExistingPlatformData = Texture->GetPlatformData())
	{
		delete ExistingPlatformData;
		Texture->SetPlatformData(nullptr);
	}

	check(!Texture->GetPlatformData());
	Texture->SetPlatformData(PlatformData);

	VOXEL_SCOPE_COUNTER("UpdateResource");
	Texture->UpdateResource();

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
	VOXEL_ENQUEUE_RENDER_COMMAND(RemoveBulkData)([WeakTexture = MakeWeakObjectPtr(Texture)](FRHICommandListImmediate& RHICmdList)
	{
		FVoxelUtilities::RunOnGameThread([=]
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

	VOXEL_SCOPE_COUNTER("UpdateResource");
	TextureArray->UpdateResource();

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

	if (!VOXEL_INLINE_COUNTER("SetCompressed", ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num())))
	{
		ensure(false);
		return false;
	}

	TArray64<uint8> RawData;
	if (!VOXEL_INLINE_COUNTER("GetRaw", ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData)))
	{
		ensure(false);
		return false;
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

	if (!VOXEL_INLINE_COUNTER("SetCompressed", ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num())))
	{
		ensure(false);
		return false;
	}

	TArray64<uint8> RawData;
	if (!VOXEL_INLINE_COUNTER("GetRaw", ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawData)))
	{
		ensure(false);
		return false;
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