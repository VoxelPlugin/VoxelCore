// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelHeightmapImporter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

TSharedPtr<FVoxelHeightmapImporter> FVoxelHeightmapImporter::MakeImporter(const FString& Path)
{
	const FString Extension = FPaths::GetExtension(Path);
	if (Extension == "raw" || Extension == "r16")
	{
		return MakeVoxelShared<FVoxelHeightmapImporter_Raw>(Path);
	}

	if (Extension == "png")
	{
		return MakeVoxelShared<FVoxelHeightmapImporter_PNG>(Path);
	}

	return nullptr;
}

bool FVoxelHeightmapImporter::Import(const FString& Path, FString& OutError, FIntPoint& OutSize, int32& OutBitDepth, TArray64<uint8>& OutData)
{
	const TSharedPtr<FVoxelHeightmapImporter> Importer = MakeImporter(Path);
	if (!Importer)
	{
		OutError = "Unknown file extension: " + FPaths::GetExtension(Path);
		return false;
	}

	if (!Importer->Import())
	{
		OutError = Importer->Error;
		return false;
	}
	ensure(Importer->Error.IsEmpty());

	OutSize = Importer->Size;
	OutBitDepth = Importer->BitDepth;
	OutData = MoveTemp(Importer->Data);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelHeightmapImporter_PNG::Import()
{
	VOXEL_FUNCTION_COUNTER();

	TArray64<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *Path))
	{
		Error = "Failed to load " + Path;
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ensure(ImageWrapper))
	{
		return false;
	}

	if (!ImageWrapper->SetCompressed(RawData.GetData(), RawData.Num()))
	{
		Error = "Failed to decode " + Path + " as a png";
		return false;
	}

	if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
	{
		Error = Path + " needs to be a grayscale png";
		return false;
	}

	if (ImageWrapper->GetBitDepth() != 8 && ImageWrapper->GetBitDepth() != 16)
	{
		Error = Path + " needs to be an 8 bit or 16 bit png";
		return false;
	}

	Size.X = ImageWrapper->GetWidth();
	Size.Y = ImageWrapper->GetHeight();

	BitDepth = ImageWrapper->GetBitDepth();

	if (!ImageWrapper->GetRaw(ERGBFormat::Gray, BitDepth, Data))
	{
		Error = Path + ": failed to decompress png data";
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelHeightmapImporter_Raw::Import()
{
	VOXEL_FUNCTION_COUNTER();

	TArray64<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *Path))
	{
		Error = "Failed to load " + Path;
		return false;
	}

	if (RawData.Num() % 2 != 0)
	{
		Error = "Invalid file size " + Path + ": possibly not 16 bit?";
		return false;
	}

	const int64 NumPixels = RawData.Num() / 2;
	const int32 SquareSize = FMath::TruncToInt(FMath::Sqrt(double(NumPixels)));
	if (NumPixels != SquareSize * SquareSize)
	{
		Error = "Invalid file size " + Path + ": is it a 16 bit raw with the same height and width?";
		return false;
	}

	Size.X = SquareSize;
	Size.Y = SquareSize;
	BitDepth = 16;
	Data = RawData;

	return true;
}