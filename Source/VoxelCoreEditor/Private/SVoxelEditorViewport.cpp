// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelEditorViewport.h"
#include "VoxelViewportInterface.h"
#include "AdvancedPreviewScene.h"
#include "AssetEditorModeManager.h"
#include "PreviewProfileController.h"
#include "SEditorViewportToolBarMenu.h"

#if VOXEL_ENGINE_VERSION >= 506
#include "ToolMenus.h"
#include "ToolMenuContext.h"
#include "AdvancedPreviewSceneMenus.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#endif

FVoxelEditorViewportClient::FVoxelEditorViewportClient(
	FEditorModeTools* EditorModeTools,
	const TSharedRef<SVoxelEditorViewport>& Viewport,
	const TSharedRef<FAdvancedPreviewScene>& PreviewScene,
	const TSharedRef<IVoxelViewportInterface>& Interface)
	: FEditorViewportClient(EditorModeTools, &PreviewScene.Get(), Viewport)
	, PreviewScene(PreviewScene)
	, WeakInterface(Interface)
{
	static_cast<FAssetEditorModeManager&>(*ModeTools).SetPreviewScene(&PreviewScene.Get());
}

void FVoxelEditorViewportClient::Tick(const float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FVoxelEditorViewportClient::Draw(
	const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	if (const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin())
	{
		Interface->Draw(View, PDI);
	}

	FEditorViewportClient::Draw(View, PDI);
}

void FVoxelEditorViewportClient::DrawCanvas(
	FViewport& InViewport,
	FSceneView& View,
	FCanvas& Canvas)
{
	if (const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin())
	{
		Interface->DrawCanvas(InViewport, View, Canvas);
	}

	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);
}

bool FVoxelEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = FEditorViewportClient::InputKey(EventArgs);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);

	bHandled |= PreviewScene->HandleInputKey(EventArgs);

	return bHandled;
}

#if VOXEL_ENGINE_VERSION < 506
bool FVoxelEditorViewportClient::InputAxis(
	FViewport* InViewport,
	const FInputDeviceId DeviceID,
	const FKey Key,
	const float Delta,
	const float DeltaTime,
	const int32 NumSamples,
	const bool bGamepad)
#else
bool FVoxelEditorViewportClient::InputAxis(const FInputKeyEventArgs& Args)
#endif
{
	if (bDisableInput)
	{
		return true;
	}

#if VOXEL_ENGINE_VERSION < 506
	if (PreviewScene->HandleViewportInput(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad))
#else
	if (PreviewScene->HandleViewportInput(Args.Viewport, Args.InputDevice, Args.Key, Args.AmountDepressed, Args.DeltaTime, Args.NumSamples, Args.IsGamepad()))
#endif
	{
		Invalidate();
		return true;
	}

#if VOXEL_ENGINE_VERSION < 506
	return FEditorViewportClient::InputAxis(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad);
#else
	return FEditorViewportClient::InputAxis(Args);
#endif
}

UE::Widget::EWidgetMode FVoxelEditorViewportClient::GetWidgetMode() const
{
	return UE::Widget::WM_Max;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelEditorViewportToolbar::Construct(
	const FArguments& Args,
	const TSharedRef<IVoxelViewportInterface>& Interface,
	const TSharedPtr<ICommonEditorViewportToolbarInfoProvider>& InfoProvider)
{
	WeakInterface = Interface;

	SCommonEditorViewportToolbarBase::Construct(
		SCommonEditorViewportToolbarBase::FArguments()
		.PreviewProfileController(MakeShared<FPreviewProfileController>()), InfoProvider);
}

void SVoxelEditorViewportToolbar::ExtendLeftAlignedToolbarSlots(
	const TSharedPtr<SHorizontalBox> MainBoxPtr,
	const TSharedPtr<SViewportToolBar> ParentToolBarPtr) const
{
	if (!MainBoxPtr)
	{
		return;
	}

	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();
	if (!Interface)
	{
		return;
	}

	Interface->PopulateToolBar(MainBoxPtr.ToSharedRef(), ParentToolBarPtr);

	if (!Interface->ShowTransformToolbar())
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

					return FText::AsNumber(Viewport->GetViewportClient()->GetCameraSpeed());
				})
				.OnGetMenuContent(ConstCast(this), &SVoxelEditorViewportToolbar::FillCameraSpeedMenu);

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
						FOnGetContent::CreateSP(ConstCast(this), &SVoxelEditorViewportToolbar::FillCameraSpeedMenu),
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

TSharedRef<SWidget> SVoxelEditorViewportToolbar::FillCameraSpeedMenu()
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

					return FText::AsNumber(Viewport->GetViewportClient()->GetCameraSpeed());
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

void SVoxelEditorViewport::Construct(
	const FArguments& Args,
	const TSharedRef<FAdvancedPreviewScene>& NewPreviewScene,
	const TSharedRef<IVoxelViewportInterface>& Interface)
{
	StatsText = Args._StatsText;
	PreviewScene = NewPreviewScene;
	WeakInterface = Interface;

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void SVoxelEditorViewport::OnFocusViewportToSelection()
{
	GetViewportClient()->FocusViewportOnBox(GetComponentBounds());
}

TSharedRef<FEditorViewportClient> SVoxelEditorViewport::MakeEditorViewportClient()
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();
	if (!ensure(Interface))
	{
		return MakeShared<FEditorViewportClient>(nullptr);
	}

	TOptional<float> InitialViewDistance = Interface->GetInitialViewDistance();
	if (InitialViewDistance.IsSet() &&
		!ensure(FMath::IsFinite(InitialViewDistance.GetValue())))
	{
		InitialViewDistance.Reset();
	}

	const FBox Bounds = GetComponentBounds();

	FEditorModeTools* EditorModeTools = Interface->GetEditorModeTools();

	const TSharedRef<FVoxelEditorViewportClient> ViewportClient = MakeShared<FVoxelEditorViewportClient>(
		EditorModeTools,
		SharedThis(this),
		PreviewScene.ToSharedRef(),
		Interface.ToSharedRef());

	ViewportClient->SetRealtime(true);
	ViewportClient->SetViewRotation(Interface->GetInitialViewRotation());

	ViewportClient->SetViewLocationForOrbiting(
		Bounds.GetCenter(),
		InitialViewDistance.Get(Bounds.GetExtent().GetMax() * 2));

	return ViewportClient;
}

#if VOXEL_ENGINE_VERSION >= 506
TSharedPtr<SWidget> SVoxelEditorViewport::BuildViewportToolbar()
{
	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();
	if (!ensure(Interface))
	{
		return SNullWidget::NullWidget;
	}

	const FName ViewportToolbarName = FName(Interface->GetToolbarName() + ".ViewportToolbar");

	if (!UToolMenus::Get()->IsMenuRegistered(ViewportToolbarName))
	{
		UToolMenu* const ViewportToolbarMenu = UToolMenus::Get()->RegisterMenu(ViewportToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);

		ViewportToolbarMenu->StyleName = "ViewportToolbar";

		FToolMenuSection& LeftSection = ViewportToolbarMenu->AddSection("Left");
		if (Interface->ShowTransformToolbar())
		{
			LeftSection.AddEntry(UE::UnrealEd::CreateTransformsSubmenu());
			LeftSection.AddEntry(UE::UnrealEd::CreateSnappingSubmenu());
		}

		{
			FToolMenuSection& RightSection = ViewportToolbarMenu->AddSection("Right");
			RightSection.Alignment = EToolMenuSectionAlign::Last;

			RightSection.AddEntry(UE::UnrealEd::CreateCameraSubmenu(UE::UnrealEd::FViewportCameraMenuOptions().ShowAll()));

			{
				{
					const FName ParentSubmenuName = "UnrealEd.ViewportToolbar.View";
					if (!UToolMenus::Get()->IsMenuRegistered(ParentSubmenuName))
					{
						UToolMenus::Get()->RegisterMenu(ParentSubmenuName);
					}

					UToolMenus::Get()->RegisterMenu("StaticMeshEditor.ViewportToolbar.ViewModes", ParentSubmenuName);
				}

				RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
			}

			RightSection.AddEntry(UE::UnrealEd::CreateDefaultShowSubmenu());
			RightSection.AddEntry(UE::UnrealEd::CreatePerformanceAndScalabilitySubmenu());

			{
				const FName PreviewSceneMenuName = "StaticMeshEditor.ViewportToolbar.AssetViewerProfile";
				RightSection.AddEntry(UE::UnrealEd::CreateAssetViewerProfileSubmenu());
				UE::AdvancedPreviewScene::Menus::ExtendAdvancedPreviewSceneSettings(PreviewSceneMenuName);
				UE::UnrealEd::ExtendPreviewSceneSettingsWithTabEntry(PreviewSceneMenuName);
			}
		}

		ViewportToolbarMenu->AddDynamicSection("Last", MakeLambdaDelegate([](UToolMenu* NewMenu)
		{
			const UVoxelEditorViewportContext* Context = NewMenu->FindContext<UVoxelEditorViewportContext>();
			if (!Context)
			{
				return;
			}

			const TSharedPtr<IVoxelViewportInterface> PinnedInterface = Context->WeakInterface.Pin();
			if (!PinnedInterface)
			{
				return;
			}

			PinnedInterface->ExtendToolbar(*NewMenu);
		}));
	}

	FToolMenuContext ViewportToolbarContext;
	{
		ViewportToolbarContext.AppendCommandList(PreviewScene->GetCommandList());
		ViewportToolbarContext.AppendCommandList(GetCommandList());

		{
			UUnrealEdViewportToolbarContext* const ContextObject = UE::UnrealEd::CreateViewportToolbarDefaultContext(SharedThis(this));

			ContextObject->bShowCoordinateSystemControls = false;

			ContextObject->AssetEditorToolkit = Interface->GetEditorToolkit();
			ContextObject->PreviewSettingsTabId = Interface->GetPreviewSettingsTabId();

			ViewportToolbarContext.AddObject(ContextObject);
		}
	}
	{
		UVoxelEditorViewportContext* Context = NewObject<UVoxelEditorViewportContext>();
		Context->WeakInterface = Interface;
		ViewportToolbarContext.AddObject(Context);
	}

	return UToolMenus::Get()->GenerateWidget(ViewportToolbarName, ViewportToolbarContext);
}
#endif

TSharedPtr<SWidget> SVoxelEditorViewport::MakeViewportToolbar()
{
	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();
	if (!ensure(Interface))
	{
		return nullptr;
	}

	return SNew(SVoxelEditorViewportToolbar, Interface.ToSharedRef(), SharedThis(this));
}

void SVoxelEditorViewport::PopulateViewportOverlays(const TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();
	if (!Interface)
	{
		return;
	}

	Overlay->AddSlot()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Left)
#if VOXEL_ENGINE_VERSION >= 506
	.Padding(MakeAttributeLambda([]
	{
		if (UE::UnrealEd::ShowOldViewportToolbars())
		{
			return FMargin(6.f, 36.f, 6.f, 6.f);
		}

		return FMargin(6.f);
	}))
#else
	.Padding(FMargin(6.f, 36.f, 6.f, 6.f))
#endif
	[
		SNew(SBorder)
		.Visibility_Lambda([this]
		{
			return StatsText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		})
		.BorderImage(FAppStyle::Get().GetBrush("FloatingBorder"))
		.Padding(4.f)
		[
			SNew(SRichTextBlock)
			.Text(StatsText)
		]
	];

	Interface->PopulateOverlay(Overlay);
}

EVisibility SVoxelEditorViewport::GetTransformToolbarVisibility() const
{
	const TSharedPtr<IVoxelViewportInterface> Interface = WeakInterface.Pin();

	if (Interface &&
		!Interface->ShowTransformToolbar())
	{
		return EVisibility::Collapsed;
	}

	return SEditorViewport::GetTransformToolbarVisibility();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SEditorViewport> SVoxelEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SVoxelEditorViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

void SVoxelEditorViewport::OnFloatingButtonClicked()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FBox SVoxelEditorViewport::GetComponentBounds() const
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