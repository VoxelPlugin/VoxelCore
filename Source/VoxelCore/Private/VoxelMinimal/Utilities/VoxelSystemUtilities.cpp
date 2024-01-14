// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelZipReader.h"
#include "VoxelPluginVersion.h"
#include "Interfaces/IPluginManager.h"
#include "Application/ThrottleManager.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "EditorViewportClient.h"
#endif

void FVoxelSystemUtilities::DelayedCall(TFunction<void()> Call, const float Delay)
{
	// Delay will be inaccurate if not on game thread but that's fine
	FVoxelUtilities::RunOnGameThread([=]
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float)
		{
			VOXEL_FUNCTION_COUNTER();
			Call();
			return false;
		}), Delay);
	});
}

IPlugin& FVoxelSystemUtilities::GetPlugin()
{
	static TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Voxel");
	if (!Plugin)
	{
		Plugin = IPluginManager::Get().FindPlugin("VoxelCore");
	}
	if (!Plugin)
	{
		Plugin = IPluginManager::Get().FindPlugin("Voxel-dev");
	}
	if (!Plugin)
	{
		for (const TSharedRef<IPlugin>& OtherPlugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (OtherPlugin->GetName().StartsWith("Voxel-2"))
			{
				ensure(!Plugin);
				Plugin = OtherPlugin;
			}
		}
	}
	return *Plugin;
}

FVoxelPluginVersion FVoxelSystemUtilities::GetPluginVersion()
{
	FString VersionName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-PluginVersionName="), VersionName))
	{
		VersionName = GetPlugin().GetDescriptor().VersionName;
	}

	if (VersionName == "Unknown")
	{
		return {};
	}

	FVoxelPluginVersion Version;
	ensure(Version.Parse(VersionName));

	return Version;
}

FString FVoxelSystemUtilities::GetAppDataCache()
{
	static FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")) / "UnrealEngine" / "VoxelPlugin";
	return Path;
}

void FVoxelSystemUtilities::CleanupFileCache(const FString& Path, const int64 MaxSize)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Path, TEXT("*"), true, false);

	int64 TotalSize = 0;
	for (const FString& File : Files)
	{
		TotalSize += IFileManager::Get().FileSize(*File);
	}

	while (
		TotalSize > MaxSize &&
		ensure(Files.Num() > 0))
	{
		FString OldestFile;
		FDateTime OldestFileTimestamp;
		for (const FString& File : Files)
		{
			const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*File);

			if (OldestFile.IsEmpty() ||
				Timestamp < OldestFileTimestamp)
			{
				OldestFile = File;
				OldestFileTimestamp = Timestamp;
			}
		}
		LOG_VOXEL(Log, "Deleting %s", *OldestFile);

		TotalSize -= IFileManager::Get().FileSize(*OldestFile);
		ensure(IFileManager::Get().Delete(*OldestFile));
		ensure(Files.Remove(OldestFile));
	}
}

FString FVoxelSystemUtilities::Unzip(const TArray<uint8>& Data, TMap<FString, TVoxelArray64<uint8>>& OutFiles)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FVoxelZipReader> ZipReader = FVoxelZipReader::Create(Data);
	if (!ZipReader)
	{
		return "Failed to unzip";
	}

	for (const FString& File : ZipReader->GetFiles())
	{
		TVoxelArray64<uint8> FileData;
		if (!ZipReader->TryLoad(File, FileData))
		{
			return "Failed to unzip " + File;
		}

		ensure(!OutFiles.Contains(File));
		OutFiles.Add(File, MoveTemp(FileData));
	}

	return {};
}

#if WITH_EDITOR
void FVoxelSystemUtilities::EnsureViewportIsUpToDate()
{
	VOXEL_FUNCTION_COUNTER();

	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		// No need to do anything, slate is not throttling
		return;
	}

	const FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
	{
		return;
	}

	const FViewportClient* Client = Viewport->GetClient();
	if (!Client)
	{
		return;
	}

	for (FEditorViewportClient* EditorViewportClient : GEditor->GetAllViewportClients())
	{
		EditorViewportClient->Invalidate(false, false);
	}
}
#endif