// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class SVoxelEditorViewport;
class FAdvancedPreviewScene;
class IVoxelViewportInterface;

class FVoxelEditorViewportClient : public FEditorViewportClient
{
public:
	const TSharedRef<FAdvancedPreviewScene> PreviewScene;
	const TWeakPtr<IVoxelViewportInterface> WeakInterface;

	FVoxelEditorViewportClient(
		FEditorModeTools* EditorModeTools,
		const TSharedRef<SVoxelEditorViewport>& Viewport,
		const TSharedRef<FAdvancedPreviewScene>& PreviewScene,
		const TSharedRef<IVoxelViewportInterface>& Interface);

	//~ Begin FEditorViewportClient Interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
#if VOXEL_ENGINE_VERSION < 506
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceID, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad) override;
#else
	virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
#endif
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	//~ End FEditorViewportClient Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SVoxelEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	VOXEL_SLATE_ARGS()
	{
	};

	void Construct(
		const FArguments& Args,
		const TSharedRef<IVoxelViewportInterface>& Interface,
		const TSharedPtr<ICommonEditorViewportToolbarInfoProvider>& InfoProvider);

	//~ Begin SCommonEditorViewportToolbarBase Interface
	virtual void ExtendLeftAlignedToolbarSlots(
		TSharedPtr<SHorizontalBox> MainBoxPtr,
		TSharedPtr<SViewportToolBar> ParentToolBarPtr) const override;
	//~ End SCommonEditorViewportToolbarBase Interface

private:
	TWeakPtr<IVoxelViewportInterface> WeakInterface;

	TSharedPtr<SSlider> CamSpeedSlider;
	mutable TSharedPtr<SSpinBox<float>> CamSpeedScalarBox;

	TSharedRef<SWidget> FillCameraSpeedMenu();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SVoxelEditorViewport
	: public SEditorViewport
	, public ICommonEditorViewportToolbarInfoProvider
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ATTRIBUTE(FText, StatsText)
	};

	void Construct(
		const FArguments& Args,
		const TSharedRef<FAdvancedPreviewScene>& NewPreviewScene,
		const TSharedRef<IVoxelViewportInterface>& Interface);

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
	TAttribute<FText> StatsText;
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TWeakPtr<IVoxelViewportInterface> WeakInterface;

	FBox GetComponentBounds() const;
};