// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Interfaces/IPluginManager.h"
#include "VoxelTextureUtilitiesHelper.generated.h"

UCLASS()
class UVoxelTextureUtilitiesHelper : public UObject
{
	GENERATED_BODY()

public:
	UVoxelTextureUtilitiesHelper()
	{
		{
			static ConstructorHelpers::FObjectFinder<UTexture2D> TextureFinder(
				TEXT("/Script/Engine.Texture2D'/Engine/EngineResources/DefaultTexture.DefaultTexture'"));
			ensure(TextureFinder.Object);
		}

		if (IPluginManager::Get().FindPlugin("Voxel"))
		{
			static ConstructorHelpers::FObjectFinder<UTexture2DArray> TextureFinder(
				TEXT("/Script/Engine.Texture2DArray'/Voxel/Default/DefaultTextureArray.DefaultTextureArray'"));
			ensure(TextureFinder.Object);
		}
	}
};