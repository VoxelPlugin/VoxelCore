// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class ModuleRules_Voxel : ModuleRules
{
	protected bool bDevWorkflow = false;
	protected bool bVoxelDebugInEditor = false;
	protected bool bGeneratingResharperProjectFiles = false;
	protected bool bStrictIncludes = false;

	public ModuleRules_Voxel(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		// C++ 20 and 17 have incompatible lambda captures
		// C++ 20 wants [=, this]
		// C++ 17 wants [=]
		CppStandard = CppStandardVersion.Cpp17;

		string AppDataPath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "/Unreal Engine/";

		bVoxelDebugInEditor = File.Exists(PluginDirectory + "/../VoxelDebugInEditor.txt");

		// Detect if Resharper is being used
		// Be careful not to enable this all the time -Rider is passed otherwise
		// a lot of unnecessary rebuilds are triggered when using Rider
		bGeneratingResharperProjectFiles =
			Environment.CommandLine.Contains(" -Rider ") &&
			File.Exists(AppDataPath + "VoxelDevWorkflow_Resharper.txt");

		if (bGeneratingResharperProjectFiles)
		{
			PCHUsage = PCHUsageMode.NoPCHs;
			PublicDefinitions.Add("INTELLISENSE_PARSER=1");
		}

		bDevWorkflow = Target.Configuration switch
		{
			UnrealTargetConfiguration.Debug => true,
			UnrealTargetConfiguration.DebugGame => File.Exists(AppDataPath + "VoxelDevWorkflow_Debug.txt") || File.Exists(PluginDirectory + "/../VoxelDevWorkflow_Debug.txt"),
			_ => File.Exists(AppDataPath + "VoxelDevWorkflow_Dev.txt") || File.Exists(PluginDirectory + "/../VoxelDevWorkflow_Dev.txt")
		};

		// Never enable dev workflow when packaging plugin
		// or if we're installed in the Marketplace folder
		if (PluginDirectory.Contains("HostProject") ||
			PluginDirectory.Contains("Marketplace"))
		{
			bDevWorkflow = false;
		}

		if (bDevWorkflow)
		{
			bUseUnity = File.Exists(PluginDirectory + "/../EnableUnity.txt");
		}
		else
		{
			// Unoptimized voxel code is _really_ slow, hurting iteration times
			// for projects with VP as a project plugin using DebugGame
			OptimizeCode = CodeOptimization.Always;
			bUseUnity = true;
		}

		bStrictIncludes = File.Exists(PluginDirectory + "/../StrictIncludes.txt");

		if (bStrictIncludes)
		{
			bUseUnity = false;
			PCHUsage = PCHUsageMode.NoPCHs;
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"RHI",
				"Engine",
				"InputCore",
				"RenderCore",
				"DeveloperSettings",
			});

		if (GetType().Name != "VoxelCore")
		{
			PublicDependencyModuleNames.Add("VoxelCore");
		}

		if (GetType().Name.EndsWith("Editor"))
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"SlateCore",
					"EditorStyle",
					"PropertyEditor",
                    "EditorFramework",
                }
			);

			if (GetType().Name != "VoxelCoreEditor")
			{
				PublicDependencyModuleNames.Add("VoxelCoreEditor");
			}
		}

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
			);
		}

		PrivateIncludePathModuleNames.Add("DerivedDataCache");

		if (!Target.bBuildRequiresCookedData)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"DerivedDataCache",
				});
		}

		if (bGeneratingResharperProjectFiles)
		{
			// ISPC support for R#
			PublicIncludePaths.Add(Path.Combine(
				PluginDirectory,
				"Intermediate",
				"Build",
				Target.Platform.ToString(),
				"x64",
				Target.bBuildEditor ? "UnrealEditor" : "UnrealGame",
				Target.Configuration.ToString(),
				GetType().Name));
		}
	}
}

public class VoxelCore : ModuleRules_Voxel
{
	public VoxelCore(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;

		if (!bUseUnity &&
		    !bStrictIncludes)
		{
			PrivatePCHHeaderFile = "Public/VoxelMinimal.h";

			Console.WriteLine("Using Voxel shared PCH");
			SharedPCHHeaderFile = "Public/VoxelMinimal.h";
		}

		if (bDevWorkflow &&
			(Target.Configuration == UnrealTargetConfiguration.Debug ||
			 Target.Configuration == UnrealTargetConfiguration.DebugGame))
		{
			PublicDefinitions.Add("VOXEL_DEBUG=1");
		}
		else if (
			bVoxelDebugInEditor &&
			Target.bBuildEditor)
		{
			PublicDefinitions.Add("VOXEL_DEBUG=WITH_EDITOR");
		}
		else
		{
			PublicDefinitions.Add("VOXEL_DEBUG=0");
		}

		PublicDefinitions.Add("VOXEL_NO_SOURCE=0");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"TraceLog",
				"Renderer",
				"Projects",
				"ApplicationCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"zlib",
				"UElibPNG",
				"HTTP",
				"Slate",
				"SlateCore",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UATHelper",
				}
			);
		}

		SetupModulePhysicsSupport(Target);
	}
}