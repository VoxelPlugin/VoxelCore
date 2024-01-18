// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorStyle.h"
#include "VoxelEditorMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

VOXEL_CONSOLE_COMMAND(
	ReinitializeStyle,
	"voxel.editor.ReinitializeStyle",
	"")
{
	FVoxelEditorStyle::ReinitializeStyle();
}

VOXEL_CONSOLE_COMMAND(
	ReloadTextures,
	"voxel.editor.ReloadTextures",
	"")
{
	FVoxelEditorStyle::ReloadTextures();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSlateStyleSet::FVoxelSlateStyleSet(const FName Name)
	: FSlateStyleSet(Name)
{
	FSlateStyleSet::SetContentRoot(FVoxelUtilities::GetPlugin().GetBaseDir() / TEXT("Resources/EditorIcons"));
	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TArray<FVoxelStyleSetFactory> GVoxelStyleSetFactories;

class FVoxelGlobalSlateStyleSet : public FVoxelSlateStyleSet
{
public:
	FVoxelGlobalSlateStyleSet()
		: FVoxelSlateStyleSet("VoxelStyle")
	{
		for (const FVoxelStyleSetFactory& Factory : GVoxelStyleSetFactories)
		{
			CopyFrom(*Factory());
		}
	}

	void CopyFrom(const FVoxelSlateStyleSet& Other)
	{
		VOXEL_FUNCTION_COUNTER();

#define Copy(Variable) \
		for (const auto& It : Other.Variable) \
		{ \
			Variable.Add(It.Key, It.Value); \
		} \
		ConstCast(Other.Variable).Empty(); // Brushes are freed on destruction

		Copy(WidgetStyleValues);
		Copy(FloatValues);
		Copy(Vector2DValues);
		Copy(ColorValues);
		Copy(SlateColorValues);
		Copy(MarginValues);
		Copy(BrushResources);
		Copy(Sounds);
		Copy(FontInfoResources);
		Copy(DynamicBrushes);

#undef Copy
	}
};
TSharedPtr<FVoxelGlobalSlateStyleSet> GVoxelGlobalStyleSet;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const FSlateStyleSet& FVoxelEditorStyle::Get()
{
	if (!GVoxelGlobalStyleSet)
	{
		GVoxelGlobalStyleSet = MakeVoxelShared<FVoxelGlobalSlateStyleSet>();

		GOnVoxelModuleUnloaded_DoCleanup.AddLambda([]
		{
			if (GVoxelGlobalStyleSet)
			{
				Shutdown();
			}
		});
	}
	return *GVoxelGlobalStyleSet;
}

void FVoxelEditorStyle::AddFactory(const FVoxelStyleSetFactory& Factory)
{
	GVoxelStyleSetFactories.Add(Factory);

	if (GVoxelGlobalStyleSet)
	{
		GVoxelGlobalStyleSet->CopyFrom(*Factory());
	}
}

void FVoxelEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FVoxelEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

void FVoxelEditorStyle::Shutdown()
{
	Unregister();
	GVoxelGlobalStyleSet.Reset();
}

void FVoxelEditorStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

void FVoxelEditorStyle::ReinitializeStyle()
{
	Shutdown();
	GVoxelGlobalStyleSet = MakeVoxelShared<FVoxelGlobalSlateStyleSet>();

	Register();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Run this last, after all styles are registered
VOXEL_RUN_ON_STARTUP(RegisterVoxelEditorStyle, EditorCommandlet, -999)
{
	FVoxelEditorStyle::Register();
}

VOXEL_INITIALIZE_STYLE(EditorBase)
{
	Set("VoxelIcon", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon16x16));
	Set("VoxelEdMode", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon40x40));
	Set("VoxelEdMode.Small", new IMAGE_BRUSH("UIIcons/VoxelIcon_40x", CoreStyleConstants::Icon16x16));
}