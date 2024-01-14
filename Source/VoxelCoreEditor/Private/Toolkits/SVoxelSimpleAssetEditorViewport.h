// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class SSlider;
class SRichTextBlock;
class FAdvancedPreviewScene;
class SVoxelSimpleAssetEditorViewport;
struct FVoxelSimpleAssetToolkit;

class FVoxelSimpleAssetEditorViewportClient : public FEditorViewportClient
{
public:
	FAdvancedPreviewScene& PreviewScene;

	FVoxelSimpleAssetEditorViewportClient(
		FAdvancedPreviewScene* PreviewScene,
		const TWeakPtr<SVoxelSimpleAssetEditorViewport>& Viewport);

	// TODO Shared voxel base
	//~ Begin FEditorViewportClient Interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceID, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad) override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	//~ End FEditorViewportClient Interface

	void SetToolkit(const TWeakPtr<FVoxelSimpleAssetToolkit>& Toolkit);

private:
	TWeakPtr<FVoxelSimpleAssetToolkit> WeakToolkit;
};


class SVoxelSimpleAssetEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(TWeakPtr<FVoxelSimpleAssetToolkit>, Toolkit)
	};

	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

private:
	virtual void ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const override;
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;

private:
	TSharedRef<SWidget> FillCameraSpeedMenu();

private:
	TWeakPtr<FVoxelSimpleAssetToolkit> WeakToolkit;

	TSharedPtr<SSlider> CamSpeedSlider;
	mutable TSharedPtr<SSpinBox<float>> CamSpeedScalarBox;
};

class SVoxelSimpleAssetEditorViewport
	: public SEditorViewport
	, public ICommonEditorViewportToolbarInfoProvider
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(TSharedPtr<FAdvancedPreviewScene>, PreviewScene)
		SLATE_ARGUMENT(FRotator, InitialViewRotation)
		SLATE_ARGUMENT(TOptional<float>, InitialViewDistance)
		SLATE_ARGUMENT(TSharedPtr<FVoxelSimpleAssetToolkit>, Toolkit)
	};

	void Construct(const FArguments& Args);
	void UpdateStatsText(const FString& NewText);

protected:
	//~ Begin SEditorViewport Interface
	virtual void OnFocusViewportToSelection() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	//~ End SEditorViewport Interface

	//~ Begin ICommonEditorViewportToolbarInfoProvider Interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	//~ End ICommonEditorViewportToolbarInfoProvider Interface

private:
	FRotator InitialViewRotation;
	TOptional<float> InitialViewDistance;
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<SRichTextBlock> OverlayText;
	TWeakPtr<FVoxelSimpleAssetToolkit> WeakToolkit;

	bool bStatsVisible = false;
	bool bShowFullTransformsToolbar = false;

	FBox GetComponentBounds() const;
};