// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelToolkit.h"
#include "VoxelViewport.h"
#include "VoxelViewportInterface.h"
#include "VoxelSimpleAssetToolkit.generated.h"

USTRUCT()
struct VOXELCOREEDITOR_API FVoxelSimpleAssetToolkit
	: public FVoxelToolkit
	, public IVoxelViewportInterface
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	//~ Begin FVoxelToolkit Interface
	virtual void Initialize() override;
	virtual TSharedPtr<FTabManager::FLayout> GetLayout() const override;
	virtual void RegisterTabs(FRegisterTab RegisterTab) override;

	virtual void SaveDocuments() override;
	virtual void LoadDocuments() override;

	virtual void PostEditChange(const FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End FVoxelToolkit Interface

	//~ Begin IVoxelViewportInterface Interface
	virtual TSharedPtr<FAssetEditorToolkit> GetEditorToolkit() const override;
	virtual FEditorModeTools* GetEditorModeTools() const override;
#if VOXEL_ENGINE_VERSION >= 506
	virtual FString GetToolbarName() const override;
#endif
	//~ End IVoxelViewportInterface Interface

public:
	IDetailsView& GetDetailsView() const
	{
		return *PrivateDetailsView;
	}
	SVoxelViewport& GetViewport() const
	{
		return *PrivateViewport;
	}
	TSharedRef<SVoxelViewport> GetSharedViewport() const
	{
		return PrivateViewport.ToSharedRef();
	}

public:
	virtual void SetupPreview() {}
	virtual void UpdatePreview() {}
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override {}
	virtual void DrawCanvas(FViewport& Viewport, FSceneView& View, FCanvas& Canvas) override;

	virtual void PopulateToolBar(
		const TSharedRef<SHorizontalBox>& ToolbarBox,
		const TSharedPtr<SViewportToolBar>& ParentToolBar) override
	{
	}

	virtual bool SaveCameraPosition() const { return false; }

	void SetDetailsViewScrollBar(const TSharedPtr<SScrollBar>& NewScrollBar);

protected:
	static constexpr const TCHAR* DetailsTabId = TEXT("FVoxelSimpleAssetToolkit_Details");
	static constexpr const TCHAR* ViewportTabId = TEXT("FVoxelSimpleAssetToolkit_Viewport");

private:
	TSharedPtr<SVoxelViewport> PrivateViewport;
	TSharedPtr<IDetailsView> PrivateDetailsView;
	TSharedPtr<SScrollBar> DetailsViewScrollBar;
	bool bCaptureThumbnail = false;

protected:
	void BindToggleCommand(const TSharedPtr<FUICommandInfo>& UICommandInfo, bool& bValue);
	void CaptureThumbnail();

private:
	void DrawThumbnail(FViewport& InViewport);
	FObjectProperty* GetTextureProperty() const;
};