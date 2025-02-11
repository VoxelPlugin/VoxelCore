// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSimpleAssetToolkit.h"
#include "Toolkits/SVoxelSimpleAssetEditorViewport.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"

class FVoxelToolkitPreviewScene : public FAdvancedPreviewScene
{
public:
	FVoxelToolkitPreviewScene(const ConstructionValues& CVS, const float InFloorOffset = 0.0f)
		: FAdvancedPreviewScene(CVS, InFloorOffset)
	{
	}

	void SetSkyScale(const float Scale) const
	{
		SkyComponent->SetWorldScale3D(FVector(Scale));
	}
};

FVoxelSimpleAssetToolkit::~FVoxelSimpleAssetToolkit()
{
	VOXEL_FUNCTION_COUNTER();

	for (AActor* Actor : PrivateActors)
	{
		if (!ensure(Actor))
		{
			continue;
		}

		Actor->Destroy();
	}
}

void FVoxelSimpleAssetToolkit::Initialize()
{
	Super::Initialize();

	{
		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.NotifyHook = GetNotifyHook();
		Args.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
		if (DetailsViewScrollBar)
		{
			Args.ExternalScrollbar = DetailsViewScrollBar;
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PrivateDetailsView = PropertyModule.CreateDetailView(Args);
		PrivateDetailsView->SetObject(GetAsset());
	}

	PrivatePreviewScene = MakeShared<FVoxelToolkitPreviewScene>(FPreviewScene::ConstructionValues());
	PrivatePreviewScene->SetFloorVisibility(ShowFloor(), true);

	CachedWorld = GetPreviewScene().GetWorld();
	if (!ensure(CachedWorld.IsValid_Slow()))
	{
		return;
	}

	// Setup PrivateRootComponent
	{
		AActor* Actor = SpawnActor<AActor>();
		if (!ensure(Actor))
		{
			return;
		}

		PrivateRootComponent = NewObject<USceneComponent>(Actor, NAME_None, RF_Transient);
		if (!ensure(PrivateRootComponent))
		{
			return;
		}
		PrivateRootComponent->RegisterComponent();

		GetPreviewScene().AddComponent(PrivateRootComponent, FTransform::Identity);
		Actor->SetRootComponent(PrivateRootComponent);
	}

	SetupPreview();
	UpdatePreview();

	// Make sure to make the viewport after UpdatePreview so that the component bounds are correct
	Viewport = SNew(SVoxelSimpleAssetEditorViewport)
		.PreviewScene(PrivatePreviewScene)
		.InitialViewRotation(GetInitialViewRotation())
		.InitialViewDistance(GetInitialViewDistance())
		.Toolkit(SharedThis(this));

	if (!QueuedStatsText.IsEmpty())
	{
		Viewport->UpdateStatsText(QueuedStatsText);
		QueuedStatsText = {};
	}

	if (const FObjectProperty* TextureProperty = GetTextureProperty())
	{
		if (!TextureProperty->GetObjectPropertyValue_InContainer(GetAsset()))
		{
			bCaptureThumbnail = true;
		}
	}
}

TSharedPtr<FTabManager::FLayout> FVoxelSimpleAssetToolkit::GetLayout() const
{
	return FTabManager::NewLayout("FVoxelSimpleAssetToolkit_Layout_v0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
			)
		);
}

void FVoxelSimpleAssetToolkit::RegisterTabs(const FRegisterTab RegisterTab)
{
	Super::RegisterTabs(RegisterTab);

	RegisterTab(DetailsTabId, INVTEXT("Details"), "LevelEditor.Tabs.Details", PrivateDetailsView);
	RegisterTab(ViewportTabId, INVTEXT("Viewport"), "LevelEditor.Tabs.Viewports", Viewport);
}

void FVoxelSimpleAssetToolkit::Tick()
{
	Super::Tick();

	if (bPreviewQueued)
	{
		UpdatePreview();
		bPreviewQueued = false;
	}
}

void FVoxelSimpleAssetToolkit::SaveDocuments()
{
	Super::SaveDocuments();

	if (!SaveCameraPosition())
	{
		return;
	}

	if (!ensure(Viewport))
	{
		return;
	}

	const TSharedPtr<FEditorViewportClient> ViewportClient = Viewport->GetViewportClient();
	if (!ensure(ViewportClient))
	{
		return;
	}

	const UObject* Asset = GetAsset();
	if (!ensure(Asset))
	{
		return;
	}

	GConfig->SetString(
		TEXT("FVoxelSimpleAssetEditorToolkit_LastPosition"),
		*Asset->GetPathName(),
		*ViewportClient->GetViewLocation().ToString(),
		GEditorPerProjectIni);

	GConfig->SetString(
		TEXT("FVoxelSimpleAssetEditorToolkit_LastRotation"),
		*Asset->GetPathName(),
		*ViewportClient->GetViewRotation().ToString(),
		GEditorPerProjectIni);
}

void FVoxelSimpleAssetToolkit::LoadDocuments()
{
	Super::LoadDocuments();

	if (!SaveCameraPosition())
	{
		return;
	}

	if (!ensure(Viewport))
	{
		return;
	}

	const TSharedPtr<FEditorViewportClient> ViewportClient = Viewport->GetViewportClient();
	if (!ensure(ViewportClient))
	{
		return;
	}

	const UObject* Asset = GetAsset();
	if (!ensure(Asset))
	{
		return;
	}

	FString LocationString;
	if (GConfig->GetString(
		TEXT("FVoxelSimpleAssetEditorToolkit_LastPosition"),
		*Asset->GetPathName(),
		LocationString,
		GEditorPerProjectIni))
	{
		FVector Location = FVector::ZeroVector;
		Location.InitFromString(LocationString);
		ViewportClient->SetViewLocation(Location);
	}

	FString RotationString;
	if (GConfig->GetString(
		TEXT("FVoxelSimpleAssetEditorToolkit_LastRotation"),
		*Asset->GetPathName(),
		RotationString,
		GEditorPerProjectIni))
	{
		FRotator Rotation = FRotator::ZeroRotator;
		Rotation.InitFromString(RotationString);
		ViewportClient->SetViewRotation(Rotation);
	}
}

void FVoxelSimpleAssetToolkit::PostEditChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChange(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	CaptureThumbnail();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> FVoxelSimpleAssetToolkit::GetViewport() const
{
	return Viewport.ToSharedRef();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSimpleAssetToolkit::DrawThumbnail(FViewport& InViewport)
{
	if (!bCaptureThumbnail)
	{
		return;
	}
	bCaptureThumbnail = false;

	UObject* Asset = GetAsset();
	if (!ensure(Asset))
	{
		return;
	}

	const int32 SrcWidth = InViewport.GetSizeXY().X;
	const int32 SrcHeight = InViewport.GetSizeXY().Y;

	TArray<FColor> Colors;
	if (!ensure(InViewport.ReadPixels(Colors)) ||
		!ensure(Colors.Num() == SrcWidth * SrcHeight))
	{
		return;
	}

	TArray<FColor> ScaledColors;
	constexpr int32 ScaledWidth = 512;
	constexpr int32 ScaledHeight = 512;
	FImageUtils::CropAndScaleImage(SrcWidth, SrcHeight, ScaledWidth, ScaledHeight, Colors, ScaledColors);

	FCreateTexture2DParameters Params;
	Params.bDeferCompression = true;

	UTexture2D* ThumbnailImage = FImageUtils::CreateTexture2D(
		ScaledWidth,
		ScaledHeight,
		ScaledColors,
		Asset,
		{},
		RF_NoFlags,
		Params);

	if (!ensure(ThumbnailImage))
	{
		return;
	}

	const FObjectProperty* Property = GetTextureProperty();
	if (!ensure(Property))
	{
		return;
	}

	Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Asset), ThumbnailImage);

	// Broadcast an object property changed event to update the content browser
	FPropertyChangedEvent PropertyChangedEvent(nullptr);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Asset, PropertyChangedEvent);
}

void FVoxelSimpleAssetToolkit::SetDetailsViewScrollBar(const TSharedPtr<SScrollBar>& NewScrollBar)
{
	DetailsViewScrollBar = NewScrollBar;
}

void FVoxelSimpleAssetToolkit::UpdateStatsText(const FString& Message)
{
	if (!Viewport)
	{
		QueuedStatsText = Message;
		return;
	}

	Viewport->UpdateStatsText(Message);
}

void FVoxelSimpleAssetToolkit::BindToggleCommand(const TSharedPtr<FUICommandInfo>& UICommandInfo, bool& bValue)
{
	GetCommands()->MapAction(
		UICommandInfo,
		MakeWeakPtrDelegate(this, [this, &bValue]
		{
			bValue = !bValue;
			UpdatePreview();
		}),
		{},
		MakeWeakPtrDelegate(this, [&bValue]
		{
			return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}));
}

void FVoxelSimpleAssetToolkit::SetFloorScale(const FVector& Scale) const
{
	ConstCast(GetPreviewScene().GetFloorMeshComponent())->SetWorldScale3D(Scale);
}

void FVoxelSimpleAssetToolkit::SetSkyScale(const float Scale) const
{
	StaticCastSharedPtr<FVoxelToolkitPreviewScene>(PrivatePreviewScene)->SetSkyScale(Scale);
}

void FVoxelSimpleAssetToolkit::CaptureThumbnail()
{
	if (!GetTextureProperty())
	{
		return;
	}

	bCaptureThumbnail = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FObjectProperty* FVoxelSimpleAssetToolkit::GetTextureProperty() const
{
	FProperty* Property = GetAsset()->GetClass()->FindPropertyByName(STATIC_FNAME("ThumbnailTexture"));
	if (!Property)
	{
		return nullptr;
	}

	FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
	if (!ensure(ObjectProperty) ||
		!ensure(ObjectProperty->PropertyClass == UTexture2D::StaticClass()))
	{
		return nullptr;
	}

	return ObjectProperty;
}