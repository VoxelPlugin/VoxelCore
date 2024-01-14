// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

class IPlugin;
struct FVoxelPluginVersion;

struct VOXELCORE_API FVoxelSystemUtilities
{
	// Delay until next fire; 0 means "next frame"
	static void DelayedCall(TFunction<void()> Call, float Delay = 0);

	static IPlugin& GetPlugin();
	static FVoxelPluginVersion GetPluginVersion();
	static FString GetAppDataCache();
	static void CleanupFileCache(const FString& Path, int64 MaxSize);
	static FString Unzip(const TArray<uint8>& Data, TMap<FString, TVoxelArray64<uint8>>& OutFiles);

#if WITH_EDITOR
	static void EnsureViewportIsUpToDate();
#endif
};