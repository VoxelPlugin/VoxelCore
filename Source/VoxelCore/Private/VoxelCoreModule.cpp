// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelSingletonManager.h"
#include "ShaderCore.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "VoxelTaskContext.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Runtime/Online/HTTP/Private/HttpThread.h"

DEFINE_PRIVATE_ACCESS(FHttpManager, bUseEventLoop)
DEFINE_PRIVATE_ACCESS(FLegacyHttpThread, HttpThreadActiveFrameTimeInSeconds)

class FVoxelCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		LOG_VOXEL(Display, "VOXEL_DEBUG=%d", VOXEL_DEBUG);

#if WITH_EDITOR
		// Increase the HTTP tick rate
		// Makes downloads much faster
		INLINE_LAMBDA
		{
			FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();
			if (PrivateAccess::bUseEventLoop(HttpManager))
			{
				return;
			}

			LOG_VOXEL(Display, "Increasing HTTP Tick Rate");

			FHttpThreadBase* Thread = HttpManager.GetThread();
			if (!Thread)
			{
				return;
			}

			FLegacyHttpThread* LegacyThread = static_cast<FLegacyHttpThread*>(Thread);
			PrivateAccess::HttpThreadActiveFrameTimeInSeconds(*LegacyThread) = 1 / 100000.f;
		};
#endif

#if ENABLE_LOW_LEVEL_MEM_TRACKER
		GVoxelLLMDisabled = !FLowLevelMemTracker::Get().IsTagSetActive(ELLMTagSet::None);
#endif

		ensure(!GIsVoxelCoreModuleLoaded);
		GIsVoxelCoreModuleLoaded = true;

		const IPlugin& Plugin = FVoxelUtilities::GetPlugin();
		const FString Shaders = FPaths::ConvertRelativePathToFull(Plugin.GetBaseDir() / "Shaders");
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/Voxel"), Shaders);

#if WITH_EDITOR
		CreateVoxelEngineVersion();
#endif
	}
	virtual void ShutdownModule() override
	{
		ensure(GIsVoxelCoreModuleLoaded);
		GIsVoxelCoreModuleLoaded = false;
	}
	virtual void PreUnloadCallback() override
	{
		if (!ensureVoxelSlow(GVoxelGlobalTaskContext))
		{
			// Will happen if the game exits before initialization is complete
			return;
		}

		GVoxelGlobalTaskContext->CancelTasks();
		GVoxelGlobalTaskContext->FlushAllTasks();

#if !WITH_EDITOR && (PLATFORM_MAC || PLATFORM_IOS)
		// Getting exit crashes on Mac
		return;
#endif

		GOnVoxelModuleUnloaded_DoCleanup.Broadcast();

		FVoxelSingletonManager::Destroy();

		delete GVoxelSynchronousTaskContext;
		GVoxelSynchronousTaskContext = nullptr;

		delete GVoxelGlobalTaskContext;
		GVoxelGlobalTaskContext = nullptr;

		void DestroyVoxelTickers();
		DestroyVoxelTickers();

		GOnVoxelModuleUnloaded.Broadcast();
	}

private:
#if WITH_EDITOR
	static void CreateVoxelEngineVersion()
	{
		VOXEL_FUNCTION_COUNTER();

		const FString Path = FPaths::ConvertRelativePathToFull(FVoxelUtilities::GetPlugin().GetBaseDir() / "Shaders") / "VoxelEngineVersion.ush";
		const FString Text = "#define VOXEL_ENGINE_VERSION " + FString::FromInt(VOXEL_ENGINE_VERSION);

		FString ExistingText;
		FFileHelper::LoadFileToString(ExistingText, *Path);

		if (ExistingText == Text)
		{
			return;
		}

		if (FFileHelper::SaveStringToFile(Text, *Path))
		{
			return;
		}

		ensure(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, false));
		ensure(FFileHelper::SaveStringToFile(Text, *Path));
	}
#endif
};

IMPLEMENT_MODULE(FVoxelCoreModule, VoxelCore);

extern "C" void VoxelISPC_Assert(const int32 Line)
{
	ensureAlwaysMsgf(false, TEXT("%d"), Line);
}