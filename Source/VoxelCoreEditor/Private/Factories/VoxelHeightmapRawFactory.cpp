// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Factories/VoxelHeightmapRawFactory.h"
#include "EditorFramework/AssetImportData.h"

UVoxelHeightmapRawFactory::UVoxelHeightmapRawFactory()
{
	SupportedClass = UTexture2D::StaticClass();
	bEditorImport = true;

	Formats.Add(TEXT("raw;Heightmap RAW"));
	Formats.Add(TEXT("r16;Heightmap R16"));
}

bool UVoxelHeightmapRawFactory::FactoryCanImport(const FString& Filename)
{
	if (!Super::FactoryCanImport(Filename))
	{
		return false;
	}

	const int64 FileSize = IFileManager::Get().FileSize(*Filename);
	if (!ensureVoxelSlow(FileSize != -1))
	{
		return false;
	}

	if (FileSize % 2 != 0)
	{
		return false;
	}

	const int64 NumPixels = FileSize / 2;
	const int64 SquareSize = FMath::TruncToInt(FMath::Sqrt(double(NumPixels)));
	if (NumPixels != SquareSize * SquareSize)
	{
		return false;
	}

	return true;
}

UObject* UVoxelHeightmapRawFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	const FName InName,
	const EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	TArray64<uint8> SourceDataBuffer;
	if (!FFileHelper::LoadFileToArray(SourceDataBuffer, *Filename))
	{
		VOXEL_MESSAGE(Error, "Failed to load {0}", Filename);
		return nullptr;
	}

	if (SourceDataBuffer.Num() % 2 != 0)
	{
		VOXEL_MESSAGE(Error, "Invalid file size, needs to be a 16 bit raw with the same width and height\n {0}", Filename);
		return nullptr;
	}

	const int64 NumPixels = SourceDataBuffer.Num() / 2;
	const int64 Size = FMath::TruncToInt(FMath::Sqrt(double(NumPixels)));
	if (NumPixels != Size * Size)
	{
		VOXEL_MESSAGE(Error, "Invalid file size, needs to be a 16 bit raw with the same width and height\n {0}", Filename);
		return nullptr;
	}

	UTexture2D* Texture = NewObject<UTexture2D>(InParent, InClass, InName, Flags);

	Texture->Source.Init(
		Size,
		Size,
		1,
		1,
		TSF_G16,
		SourceDataBuffer.GetData());

	Texture->CompressionSettings = TC_Grayscale;
	Texture->AssetImportData->Update(Filename);
	return Texture;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelHeightmapRawFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UTexture2D* Texture = Cast<UTexture2D>(Obj);
	if (!Texture)
	{
		return false;
	}

	const TArray<FString> Filenames = Texture->AssetImportData->ExtractFilenames();
	if (Filenames.Num() != 1)
	{
		return false;
	}

	if (!IsSupportedFileExtension(FPaths::GetExtension(Filenames[0])))
	{
		return false;
	}

	OutFilenames = Filenames;
	return true;
}

void UVoxelHeightmapRawFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (!ensureVoxelSlow(NewReimportPaths.Num() == 1))
	{
		return;
	}

	CastChecked<UTexture2D>(Obj)->AssetImportData->Update(NewReimportPaths[1]);
}

EReimportResult::Type UVoxelHeightmapRawFactory::Reimport(UObject* Obj)
{
	UTexture2D* Texture = Cast<UTexture2D>(Obj);
	if (!ensure(Texture))
	{
		return EReimportResult::Failed;
	}

	bool bCancelled = false;
	if (!ImportObject(
		Texture->GetClass(),
		Texture->GetOuter(),
		*Texture->GetName(),
		RF_Public | RF_Standalone,
		Texture->AssetImportData->GetFirstFilename(),
		nullptr,
		bCancelled))
	{
		return EReimportResult::Failed;
	}

	Texture->MarkPackageDirty();
	return EReimportResult::Succeeded;
}

int32 UVoxelHeightmapRawFactory::GetPriority() const
{
	return 1000;
}