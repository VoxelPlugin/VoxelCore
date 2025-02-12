// Copyright Voxel Plugin SAS. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class VoxelCore : ModuleRules
{
	public VoxelCore(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		IWYUSupport = IWYUSupport.None;
		bUseUnity = false;
		PrivatePCHHeaderFile = "Public/VoxelMinimal.h";

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"RHI",
				"Engine",
				"TraceLog",
				"Renderer",
				"Projects",
				"InputCore",
				"RenderCore",
				"ApplicationCore",
				"DeveloperSettings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos",
				"zlib",
				"UElibPNG",
				"HTTP",
				"Slate",
				"SlateCore",
				"Json",
				"JsonUtilities",
				"MoviePlayer",
#if UE_5_4_OR_LATER
				"EventLoop",
#endif
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"UATHelper",
					"GraphEditor",
					"MaterialEditor",
				}
			);
		}

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(EngineDirectory, "Source/Runtime/Engine/Private"),
				Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private")
			}
		);
	}
}
