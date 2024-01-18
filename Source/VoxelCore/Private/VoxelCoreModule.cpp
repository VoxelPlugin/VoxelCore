// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "ShaderCore.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Runtime/Online/HTTP/Private/HttpThread.h"

class FVoxelCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		LOG_VOXEL(Log, "VOXEL_DEBUG=%d", VOXEL_DEBUG);

#if WITH_EDITOR
		// Increase the HTTP tick rate
		// Makes downloads much faster
		{
			LOG_VOXEL(Log, "Increasing HTTP Tick Rate");

			FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();

			struct FHttpThreadHack : UE_503_SWITCH(FHttpThread, FLegacyHttpThread)
			{
				void Fixup()
				{
					HttpThreadActiveFrameTimeInSeconds = 1 / 100000.f;
				}
			};

			struct FHttpManagerHack : FHttpManager
			{
				void Fixup() const
				{
					if (!Thread)
					{
						return;
					}

					static_cast<FHttpThreadHack*>(Thread)->Fixup();
				}
			};

			static_cast<FHttpManagerHack&>(HttpManager).Fixup();
		}
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
		// Run cleanup twice in case first cleanup added voxel nodes back to the pool
		GOnVoxelModuleUnloaded_DoCleanup.Broadcast();
		GOnVoxelModuleUnloaded_DoCleanup.Broadcast();

		void DestroyVoxelSingletonManager();
		DestroyVoxelSingletonManager();

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
extern "C" void VoxelISPC_UnsupportedTargetWidth(const int32 Width)
{
	LOG_VOXEL(Fatal, "Unsupported ISPC target width: %d", Width);
}