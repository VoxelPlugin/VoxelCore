// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "VoxelMinimal/VoxelColor3.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

class UTexture2D;
class UTexture2DArray;

struct VOXELCORE_API FVoxelTextureUtilities
{
public:
	static UTexture2D* GetDefaultTexture2D();
	static UTexture2DArray* GetDefaultTexture2DArray();

public:
	static UTexture2D* CreateTexture2D(
		FName DebugName,
		int32 SizeX,
		int32 SizeY,
		bool bSRGB,
		TextureFilter Filter,
		EPixelFormat PixelFormat,
		TFunction<void(TVoxelArrayView<uint8> Data)> InitializeMip0 = nullptr,
		UTexture2D* ExistingTexture = nullptr);

	// Unload bulk data, saving CPU memory for textures that can be kept on the GPU
	static void RemoveBulkData(UTexture2D* Texture);

	static UTexture2DArray* CreateTextureArray(
		FName DebugName,
		int32 SizeX,
		int32 SizeY,
		int32 SizeZ,
		bool bSRGB,
		TextureFilter Filter,
		EPixelFormat PixelFormat,
		int32 NumMips = 1,
		TFunction<void(TVoxelArrayView<uint8> Data, int32 MipIndex)> InitializeMip = nullptr,
		UTexture2DArray* ExistingTexture = nullptr);

public:
	static TArray64<uint8> CompressPng_RGB(
		const TConstArrayView64<FVoxelColor3>& ColorData,
		int32 Width,
		int32 Height);

	static TArray64<uint8> CompressPng_Grayscale(
		const TConstArrayView64<uint16>& GrayscaleData,
		int32 Width,
		int32 Height);

	static bool UncompressPng_RGB(
		const TConstArrayView64<uint8>& CompressedData,
		TVoxelArray64<FVoxelColor3>& OutColorData,
		int32& OutWidth,
		int32& OutHeight);

	static bool UncompressPng_Grayscale(
		const TConstArrayView64<uint8>& CompressedData,
		TVoxelArray64<uint16>& OutGrayscaleData,
		int32& OutWidth,
		int32& OutHeight);

public:
	static void FullyLoadTexture(UTexture* Texture)
	{
		FullyLoadTextures({ Texture });
	}
	static void FullyLoadTextures(const TArray<UTexture*>& Textures);
};