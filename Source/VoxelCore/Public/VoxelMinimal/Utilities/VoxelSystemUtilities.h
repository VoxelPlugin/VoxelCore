// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelStaticArray.h"

class IPlugin;
struct FVoxelPluginVersion;

using FVoxelStackFrames = TVoxelStaticArray_ForceInit<void*, 14>;

FORCEINLINE uint32 GetTypeHash(const FVoxelStackFrames& StackFrames)
{
	return FVoxelUtilities::MurmurHashBytes(MakeByteVoxelArrayView(StackFrames));
}

namespace FVoxelUtilities
{
	// Default is FPlatformMisc::NumberOfWorkerThreadsToSpawn()
	// Useful to go beyond 4 threads on a server or to simulate a slower PC
	VOXELCORE_API void SetNumWorkerThreads(int32 NumWorkerThreads);
	VOXELCORE_API int32 GetNumBackgroundWorkerThreads();

	// Delay until next fire; 0 means "next frame"
	VOXELCORE_API void DelayedCall(TFunction<void()> Call, float Delay = 0);
	VOXELCORE_API FString Unzip(const TArray<uint8>& Data, TMap<FString, TVoxelArray64<uint8>>& OutFiles);

#if WITH_EDITOR
	VOXELCORE_API void EnsureViewportIsUpToDate();
#endif

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API IPlugin& GetPlugin();
	VOXELCORE_API FVoxelPluginVersion GetPluginVersion();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	VOXELCORE_API void CleanupRedirects(const FString& RedirectsPath);
#endif

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FString GetAppDataCache();
	VOXELCORE_API void CleanupFileCache(const FString& Path, int64 MaxSize);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FVoxelStackFrames GetStackFrames(int32 NumFramesToIgnore = 1);
	VOXELCORE_API TVoxelArray<FString> StackFramesToString(const FVoxelStackFrames& StackFrames);
};