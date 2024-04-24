// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Toolkits/SVoxelSimpleAssetEditorViewport.h"
#include "EditorModeManager.h"
#include "AssetEditorModeManager.h"
#include "VoxelSimpleAssetToolkit.h"
#include "PreviewProfileController.h"
#include "SEditorViewportToolBarMenu.h"

FVoxelSimpleAssetEditorViewportClient::FVoxelSimpleAssetEditorViewportClient(
	FEditorModeTools* EditorModeTools,
	FAdvancedPreviewScene* PreviewScene,
	const TWeakPtr<SVoxelSimpleAssetEditorViewport>& Viewport)
	: FEditorViewportClient(EditorModeTools, PreviewScene, Viewport)
	, PreviewScene(*PreviewScene)
{
	StaticCastSharedPtr<FAssetEditorModeManager>(ModeTools)->SetPreviewScene(PreviewScene);
}

void FVoxelSimpleAssetEditorViewportClient::Tick(const float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene.GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FVoxelSimpleAssetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const TSharedPtr<FVoxelSimpleAssetToolkit> Toolkit = WeakToolkit.Pin())
	{
		Toolkit->DrawPreview(View, PDI);
	}

	FEditorViewportClient::Draw(View, PDI);
}

void FVoxelSimpleAssetEditorViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	if (const TSharedPtr<FVoxelSimpleAssetToolkit> Toolkit = WeakToolkit.Pin())
	{
		Toolkit->DrawPreviewCanvas(InViewport, View, Canvas);
		Toolkit->DrawThumbnail(InViewport);
	}

	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);
}

bool FVoxelSimpleAssetEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = FEditorViewportClient::InputKey(EventArgs);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);

	bHandled |= PreviewScene.HandleInputKey(EventArgs);

	return bHandled;
}

bool FVoxelSimpleAssetEditorViewportClient::InputAxis(FViewport* InViewport, const FInputDeviceId DeviceID, const FKey Key, const float Delta, const float DeltaTime, const int32 NumSamples, const bool bGamepad)
{
	bool bResult = true;

	if (!bDisableInput)
	{
		bResult = PreviewScene.HandleViewportInput(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
	}

	return bResult;
}

UE::Widget::EWidgetMode FVoxelSimpleAssetEditorViewportClient::GetWidgetMode() const
{
	return UE::Widget::WM_Max;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSimpleAssetEditorViewportClient::SetToolkit(const TWeakPtr<FVoxelSimpleAssetToolkit>& Toolkit)
{
	WeakToolkit = Toolkit;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelSimpleAssetEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	WeakToolkit = InArgs._Toolkit;

	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments().PreviewProfileController(MakeVoxelShared<FPreviewProfileController>()), InInfoProvider);
}

void SVoxelSimpleAssetEditorViewportToolbar::ExtendLeftAlignedToolbarSlots(const TSharedPtr<SHorizontalBox> MainBoxPtr, const TSharedPtr<SViewportToolBar> ParentToolBarPtr) const
{
	if (!MainBoxPtr)
	{
		return;
	}

	const TSharedPtr<FVoxelSimpleAssetToolkit> Toolkit = WeakToolkit.Pin();
	if (!Toolkit)
	{
		return;
	}

	Toolkit->PopulateToolBar(MainBoxPtr, ParentToolBarPtr);

	if (!Toolkit->ShowFullTransformsToolbar())
	{
		FSlimHorizontalToolBarBuilder ToolbarBuilder(GetInfoProvider().GetViewportWidget()->GetCommandList(), FMultiBoxCustomization::None);

		ToolbarBuilder.SetStyle(&FAppStyle::Get(), "EditorViewportToolBar");
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		ToolbarBuilder.SetIsFocusable(false);

		ToolbarBuilder.BeginSection("CameraSpeed");
		{
			const TSharedRef<SEditorViewportToolbarMenu> CameraToolbarMenu =
				SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(ConstCast(this)))
				.AddMetaData<FTagMetaData>(FTagMetaData("CameraSpeedButton"))
				.ToolTipText(INVTEXT("Camera Speed"))
				.LabelIcon(FAppStyle::Get().GetBrush("EditorViewport.CamSpeedSetting"))
				.Label_Lambda([this]() -> FText
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
					   return {};
					}

					return FText::AsNumber(Viewport->GetViewportClient()->GetCameraSpeedSetting());
				})
				.OnGetMenuContent(ConstCast(this), &SVoxelSimpleAssetEditorViewportToolbar::FillCameraSpeedMenu);

			ToolbarBuilder.AddWidget(
				CameraToolbarMenu,
				STATIC_FNAME("CameraSpeed"),
				false,
				HAlign_Fill,
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddWrapperSubMenu(
						INVTEXT("Camera Speed Settings"),
						INVTEXT("Adjust the camera navigation speed"),
						FOnGetContent::CreateSP(ConstCast(this), &SVoxelSimpleAssetEditorViewportToolbar::FillCameraSpeedMenu),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.CamSpeedSetting")
					);
				}
			));
		}
		ToolbarBuilder.EndSection();

		MainBoxPtr->AddSlot()
		.Padding(4.f, 1.f)
		.HAlign(HAlign_Right)
		[
			ToolbarBuilder.MakeWidget()
		];
	}
}

TSharedRef<SWidget> SVoxelSimpleAssetEditorViewportToolbar::GenerateShowMenu() const
{
	const TSharedPtr<FVoxelSimpleAssetToolkit> Toolkit = WeakToolkit.Pin();
	if (!Toolkit)
	{
		return SNullWidget::NullWidget;
	}

	return Toolkit->PopulateToolBarShowMenu();
}

TSharedRef<SWidget> SVoxelSimpleAssetEditorViewportToolbar::FillCameraSpeedMenu()
{
	TSharedRef<SWidget> ReturnWidget = SNew(SBorder)
	.BorderImage(FAppStyle::GetBrush(("Menu.Background")))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 2.f, 60.f, 2.f))
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(INVTEXT("Camera Speed"))
			.Font(FAppStyle::GetFontStyle("MenuItem.Font"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.Padding(FMargin(0.f, 2.f))
			[
				SAssignNew(CamSpeedSlider, SSlider)
				.Value_Lambda([this]
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
						return 0.f;
					}

					return (Viewport->GetViewportClient()->GetCameraSpeedSetting() - 1.f) / (float(FEditorViewportClient::MaxCameraSpeeds) - 1.f);
				})
				.OnValueChanged_Lambda([this](const float NewValue)
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
						return;
					}

					const int32 OldSpeedSetting = Viewport->GetViewportClient()->GetCameraSpeedSetting();
					const int32 NewSpeedSetting = NewValue * (float(FEditorViewportClient::MaxCameraSpeeds) - 1.f) + 1;

					if (OldSpeedSetting != NewSpeedSetting)
					{
						Viewport->GetViewportClient()->SetCameraSpeedSetting(NewSpeedSetting);
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.f, 2.f, 0.f, 2.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
						return {};
					}

					return FText::AsNumber(Viewport->GetViewportClient()->GetCameraSpeedSetting());
				})
				.Font(FAppStyle::GetFontStyle("MenuItem.Font"))
			]
		] // Camera Speed Scalar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 2.f, 60.f, 2.f))
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(INVTEXT("Camera Speed Scalar"))
			.Font(FAppStyle::GetFontStyle("MenuItem.Font"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(FMargin(0.f, 2.f))
			[
				SAssignNew(CamSpeedScalarBox, SSpinBox<float>)
				.MinValue(1.f)
				.MaxValue(TNumericLimits<int32>::Max())
				.MinSliderValue(1.f)
				.MaxSliderValue(128.f)
				.Value_Lambda([this]
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
						return 1.f;
					}

					return Viewport->GetViewportClient()->GetCameraSpeedScalar();
				})
				.OnValueChanged_Lambda([this](const float NewValue)
				{
					const TSharedRef<SEditorViewport> Viewport = GetInfoProvider().GetViewportWidget();
					if (!Viewport->GetViewportClient().IsValid())
					{
						return;
					}

					Viewport->GetViewportClient()->SetCameraSpeedScalar(NewValue);
				})
				.ToolTipText(INVTEXT("Scalar to increase camera movement range"))
			]
		]
	];

	return ReturnWidget;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelSimpleAssetEditorViewport::Construct(const FArguments& Args)
{
	PreviewScene = Args._PreviewScene;
	InitialViewRotation = Args._InitialViewRotation;
	InitialViewDistance = Args._InitialViewDistance;
	WeakToolkit = Args._Toolkit;
	bShowFullTransformsToolbar = Args._Toolkit->ShowFullTransformsToolbar();

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void SVoxelSimpleAssetEditorViewport::UpdateStatsText(const FString& NewText)
{
	bStatsVisible = true;
	OverlayText->SetText(FText::FromString(NewText));
}

void SVoxelSimpleAssetEditorViewport::OnFocusViewportToSelection()
{
	if (PreviewScene.IsValid())
	{
		GetViewportClient()->FocusViewportOnBox(GetComponentBounds());
	}
}

TSharedRef<FEditorViewportClient> SVoxelSimpleAssetEditorViewport::MakeEditorViewportClient()
{
	if (InitialViewDistance.IsSet() &&
		!ensure(FMath::IsFinite(InitialViewDistance.GetValue())))
	{
		InitialViewDistance = {};
	}

	const FBox Bounds = GetComponentBounds();

	FEditorModeTools* EditorModeTools = nullptr;
	if (const TSharedPtr<FVoxelSimpleAssetToolkit> Toolkit = WeakToolkit.Pin())
	{
		EditorModeTools = Toolkit->GetEditorModeTools().Get();
	}

	const TSharedRef<FVoxelSimpleAssetEditorViewportClient> ViewportClient = MakeVoxelShared<FVoxelSimpleAssetEditorViewportClient>(EditorModeTools, PreviewScene.Get(), SharedThis(this));
	ViewportClient->SetRealtime(true);
	ViewportClient->SetViewRotation(InitialViewRotation);
	ViewportClient->SetViewLocationForOrbiting(Bounds.GetCenter(), InitialViewDistance.Get(Bounds.GetExtent().GetMax() * 2));
	ViewportClient->SetToolkit(WeakToolkit);

	return ViewportClient;
}

TSharedPtr<SWidget> SVoxelSimpleAssetEditorViewport::MakeViewportToolbar()
{
	return
		SNew(SVoxelSimpleAssetEditorViewportToolbar, SharedThis(this))
		.Toolkit(WeakToolkit);
}

void SVoxelSimpleAssetEditorViewport::PopulateViewportOverlays(const TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Left)
	.Padding(FMargin(6.f, 36.f, 6.f, 6.f))
	[
		SNew(SBorder)
		.Visibility_Lambda([this]
		{
			return bStatsVisible ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.BorderImage(FAppStyle::Get().GetBrush("FloatingBorder"))
		.Padding(4.f)
		[
			SAssignNew(OverlayText, SRichTextBlock)
		]
	];
}

EVisibility SVoxelSimpleAssetEditorViewport::GetTransformToolbarVisibility() const
{
	if (bShowFullTransformsToolbar)
	{
		return SEditorViewport::GetTransformToolbarVisibility();
	}

	return EVisibility::Collapsed;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SEditorViewport> SVoxelSimpleAssetEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SVoxelSimpleAssetEditorViewport::GetExtenders() const
{
	return MakeVoxelShared<FExtender>();
}

void SVoxelSimpleAssetEditorViewport::OnFloatingButtonClicked()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FBox SVoxelSimpleAssetEditorViewport::GetComponentBounds() const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(PreviewScene))
	{
		return FBox(ForceInit);
	}

	const UWorld* World = PreviewScene->GetWorld();
	if (!ensure(World))
	{
		return FBox(ForceInit);
	}

	FBox Bounds(ForceInit);
	ForEachObjectOfClass<USceneComponent>([&](const USceneComponent& Component)
	{
		if (Component.HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject) ||
			Component.GetWorld() != World ||
			!Component.GetOwner())
		{
			return;
		}

		// Force a CalcBounds for ISMs when there hasn't been any tick yet
		Bounds += Component.CalcBounds(Component.GetComponentToWorld()).GetBox();
	});
	return Bounds;
}