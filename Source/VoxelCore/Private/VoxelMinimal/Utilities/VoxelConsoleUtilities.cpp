// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#if WITH_EDITOR
#include "IUATHelperModule.h"
#include "Styling/AppStyle.h"
#endif

VOXEL_CONSOLE_COMMAND(
	ToggleNamedEvents,
	"voxel.toggleNamedEvents",
	"Toggle verbose named events (expensive!)")
{
    static bool bToggled = false;

    if (GCycleStatsShouldEmitNamedEvents == 0)
    {
        GCycleStatsShouldEmitNamedEvents++;
	}

	if (!bToggled)
	{
		StatsPrimaryEnableAdd();
	}
	else
	{
		StatsPrimaryEnableSubtract();
	}

    bToggled = !bToggled;
}

struct FVoxelUnrealInsightsLauncher
{
	static FString GetInsightsApplicationPath()
	{
		const FString Path = FPlatformProcess::GenerateApplicationPath(TEXT("UnrealInsights"), EBuildConfiguration::Development);
		return FPaths::ConvertRelativePathToFull(Path);
	}
	static void StartUnrealInsights(const FString& Path, const FString& Parameters)
	{
		if (!FPaths::FileExists(Path))
		{
			TryBuildUnrealInsightsExe(Path, Parameters);
			return;
		}

		uint32 ProcessID = 0;
		const FProcHandle Handle = FPlatformProcess::CreateProc(
			*Path,
			*Parameters,
			true,
			false,
			false,
			&ProcessID,
			0,
			nullptr,
			nullptr,
			nullptr);

		if (Handle.IsValid())
		{
			LOG_VOXEL(Log, "Launched Unreal Insights executable: %s %s", *Path, *Parameters);
		}
		else
		{
			VOXEL_MESSAGE(Error, "Could not start Unreal Insights executable at path: {0}", Path);
		}
	}
	static void TryBuildUnrealInsightsExe(const FString& Path, const FString& LaunchParameters)
	{
#if WITH_EDITOR
		VOXEL_MESSAGE(Error, "Could not find the Unreal Insights executable: {0}. Attempting to build UnrealInsights", Path);

		FString Arguments;
#if PLATFORM_WINDOWS
		FText PlatformName = INVTEXT("Windows");
		Arguments = TEXT("BuildTarget -Target=UnrealInsights -Platform=Win64");
#elif PLATFORM_MAC
		FText PlatformName = INVTEXT("Mac");
		Arguments = TEXT("BuildTarget -Target=UnrealInsights -Platform=Mac");
#elif PLATFORM_LINUX
		FText PlatformName = INVTEXT("Linux");
		Arguments = TEXT("BuildTarget -Target=UnrealInsights -Platform=Linux");
#endif

		IUATHelperModule::Get().CreateUatTask(
			Arguments,
			PlatformName,
			INVTEXT("Building Unreal Insights"),
			INVTEXT("Build Unreal Insights Task"),
			FAppStyle::GetBrush(TEXT("MainFrame.CookContent")),
			nullptr,
			[=](FString Result, double Time)
			{
				if (Result.Equals(TEXT("Completed")))
				{
#if PLATFORM_MAC
					// On Mac we generate the path again so that it includes the newly built executable.
					FString NewPath = GetInsightsApplicationPath();
					StartUnrealInsights(NewPath, LaunchParameters);
#else
					StartUnrealInsights(Path, LaunchParameters);
#endif
				}
			});
#endif
	}
};

#if STATS
VOXEL_CONSOLE_COMMAND(
	StartInsights,
	"voxel.StartInsights",
	"")
{
	GCycleStatsShouldEmitNamedEvents++;
	UE::Trace::ToggleChannel(TEXT("VoxelChannel"), true);

	FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("localhost"), nullptr);

	FVoxelUnrealInsightsLauncher::StartUnrealInsights(
		FVoxelUnrealInsightsLauncher::GetInsightsApplicationPath(),
		FTraceAuxiliary::UE_503_SWITCH(GetTraceDestination, GetTraceDestinationString)());
}

VOXEL_CONSOLE_COMMAND(
	StopInsights,
	"voxel.StopInsights",
	"")
{
	FTraceAuxiliary::Stop();

	GCycleStatsShouldEmitNamedEvents--;
	UE::Trace::ToggleChannel(TEXT("VoxelChannel"), false);
}
#endif