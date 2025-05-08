// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelSimpleAssetToolkit.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"

void FVoxelSimpleAssetToolkit::Initialize()
{
	Super::Initialize();

	PrivateViewport = SNew(SVoxelViewport);

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

	SetupPreview();
	UpdatePreview();

	// Make sure to initialize the viewport after UpdatePreview so that the component bounds are correct
	PrivateViewport->Initialize(SharedThis(this));

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
	RegisterTab(ViewportTabId, INVTEXT("Viewport"), "LevelEditor.Tabs.Viewports", PrivateViewport);
}

void FVoxelSimpleAssetToolkit::SaveDocuments()
{
	Super::SaveDocuments();

	if (!SaveCameraPosition())
	{
		return;
	}

	if (!ensure(PrivateViewport))
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
		*PrivateViewport->GetViewLocation().ToString(),
		GEditorPerProjectIni);

	GConfig->SetString(
		TEXT("FVoxelSimpleAssetEditorToolkit_LastRotation"),
		*Asset->GetPathName(),
		*PrivateViewport->GetViewRotation().ToString(),
		GEditorPerProjectIni);
}

void FVoxelSimpleAssetToolkit::LoadDocuments()
{
	Super::LoadDocuments();

	if (!SaveCameraPosition())
	{
		return;
	}

	if (!ensure(PrivateViewport))
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
		PrivateViewport->SetViewLocation(Location);
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
		PrivateViewport->SetViewRotation(Rotation);
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

void FVoxelSimpleAssetToolkit::DrawCanvas(FViewport& Viewport, FSceneView& View, FCanvas& Canvas)
{
	if (bCaptureThumbnail)
	{
		DrawThumbnail(Viewport);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSimpleAssetToolkit::SetDetailsViewScrollBar(const TSharedPtr<SScrollBar>& NewScrollBar)
{
	DetailsViewScrollBar = NewScrollBar;
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

void FVoxelSimpleAssetToolkit::DrawThumbnail(FViewport& InViewport)
{
	VOXEL_FUNCTION_COUNTER();

	ensure(bCaptureThumbnail);
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