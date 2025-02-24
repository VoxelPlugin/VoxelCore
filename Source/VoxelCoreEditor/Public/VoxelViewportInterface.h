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
	virtual FEditorModeTools* GetEditorModeTools() const
	{
		return nullptr;
	}

public:
	virtual void PopulateToolBar(
		const TSharedRef<SHorizontalBox>& ToolbarBox,
		const TSharedPtr<SViewportToolBar>& ParentToolBar)
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