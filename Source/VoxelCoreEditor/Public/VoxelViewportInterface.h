// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class SViewportToolBar;

class VOXELCOREEDITOR_API IVoxelViewportInterface
{
public:
	IVoxelViewportInterface() = default;
	virtual ~IVoxelViewportInterface() = default;

public:
	virtual bool ShowFloor() const
	{
		return true;
	}
	virtual bool ShowTransformToolbar() const
	{
		return false;
	}
	virtual FRotator GetInitialViewRotation() const
	{
		return FRotator(-20.0f, 45.0f, 0.f);
	}
	virtual TOptional<float> GetInitialViewDistance() const
	{
		return {};
	}
	virtual TOptional<float> GetMaxFocusDistance() const
	{
		return {};
	}
	virtual TSharedPtr<FAssetEditorToolkit> GetEditorToolkit() const
	{
		return nullptr;
	}
	virtual FEditorModeTools* GetEditorModeTools() const
	{
		return nullptr;
	}
#if VOXEL_ENGINE_VERSION >= 506
	virtual FName GetPreviewSettingsTabId() const
	{
		return {};
	}
	virtual FString GetToolbarName() const VOXEL_PURE_VIRTUAL({})
	virtual void ExtendToolbar(UToolMenu& ToolMenu) {}
#endif

public:
	virtual void PopulateToolBar(
		const TSharedRef<SHorizontalBox>& ToolbarBox,
		const TSharedPtr<SViewportToolBar>& ParentToolBar)
	{
	}
	virtual void PopulateOverlay(const TSharedRef<SOverlay>& Overlay)
	{
	}

public:
	virtual void Draw(
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI)
	{
	}
	virtual void DrawCanvas(
		FViewport& Viewport,
		FSceneView& View,
		FCanvas& Canvas)
	{
	}
};