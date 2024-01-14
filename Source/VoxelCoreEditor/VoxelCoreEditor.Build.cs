// Copyright Voxel Plugin, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class VoxelCoreEditor : ModuleRules_Voxel
{
    public VoxelCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
#if UE_5_2_OR_LATER
		IWYUSupport = IWYUSupport.None;
#else
		bEnforceIWYU = false;
#endif

		if (!bUseUnity &&
			!bStrictIncludes)
		{
			PrivatePCHHeaderFile = "Public/VoxelCoreEditorMinimal.h";

			Console.WriteLine("Using Voxel Editor shared PCH");
			SharedPCHHeaderFile = "Public/VoxelCoreEditorMinimal.h";
		}

		// For FDetailCategoryBuilderImpl
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Editor/PropertyEditor/Private"));

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AdvancedPreviewScene",
            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "PlacementMode",
                "MessageLog",
                "WorkspaceMenuStructure",
                "DetailCustomizations",
                "BlueprintGraph",
                "Landscape",
                "ToolMenus",
                "SceneOutliner",
            });
    }
}