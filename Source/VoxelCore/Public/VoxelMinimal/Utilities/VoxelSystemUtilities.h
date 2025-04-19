// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"

class IPlugin;
struct FVoxelPluginVersion;

using FVoxelStackFrames = TVoxelArray<void*>;

FORCEINLINE uint32 GetTypeHash(const FVoxelStackFrames& StackFrames)
{
	return FVoxelUtilities::MurmurHashView(StackFrames);
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
	VOXELCORE_API bool IsDevWorkflow();

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

	VOXELCORE_API FVoxelStackFrames GetStackFramesImpl(
		bool bEnableStats,
		int32 NumFramesToIgnore);

	VOXELCORE_API TVoxelArray<FString> StackFramesToStringImpl(
		bool bEnableStats,
		const FVoxelStackFrames& StackFrames,
		bool bIncludeFilenames);

	VOXELCORE_API FString GetPrettyCallstackImpl(
		bool bEnableStats,
		int32 NumFramesToIgnore);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE FVoxelStackFrames GetStackFrames_NoStats(const int32 NumFramesToIgnore = 2)
	{
		return GetStackFramesImpl(
			false,
			NumFramesToIgnore);
	}
	FORCEINLINE FVoxelStackFrames GetStackFrames_WithStats(const int32 NumFramesToIgnore = 2)
	{
		return GetStackFramesImpl(
			true,
			NumFramesToIgnore);
	}

	FORCEINLINE TVoxelArray<FString> StackFramesToString_NoStats(
		const FVoxelStackFrames& StackFrames,
		const bool bIncludeFilenames = true)
	{
		return StackFramesToStringImpl(
			false,
			StackFrames,
			bIncludeFilenames);
	}

	FORCEINLINE TVoxelArray<FString> StackFramesToString_WithStats(
		const FVoxelStackFrames& StackFrames,
		const bool bIncludeFilenames = true)
	{
		return StackFramesToStringImpl(
			true,
			StackFrames,
			bIncludeFilenames);
	}

	FORCEINLINE FString GetPrettyCallstack_NoStats(const int32 NumFramesToIgnore = 3)
	{
		return GetPrettyCallstackImpl(
			false,
			NumFramesToIgnore);
	}
	FORCEINLINE FString GetPrettyCallstack_WithStats(const int32 NumFramesToIgnore = 3)
	{
		return GetPrettyCallstackImpl(
			true,
			NumFramesToIgnore);
	}
};