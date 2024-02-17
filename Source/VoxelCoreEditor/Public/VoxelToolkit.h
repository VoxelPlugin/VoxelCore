// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Misc/NotifyHook.h"
#include "VoxelToolkit.generated.h"

USTRUCT()
struct VOXELCOREEDITOR_API FVoxelToolkit
	: public FVoxelVirtualStruct
#if CPP
	, public TSharedFromThis<FVoxelToolkit>
	, private FNotifyHook
	, private FEditorUndoClient
#endif
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	FVoxelToolkit() = default;
	virtual ~FVoxelToolkit() override;

	void InitializeInternal(const TSharedRef<FUICommandList>& Commands, UObject* Asset);

	const FObjectProperty* GetObjectProperty() const;

	UObject* GetAsset() const
	{
		check(CachedAssetPtr);
		return *CachedAssetPtr;
	}
	TSharedPtr<FTabManager> GetTabManager() const
	{
		return WeakTabManager.Pin();
	}

	virtual void Initialize() {}

	virtual void Tick() {}

	virtual void PostUndo() {}
	virtual void PostRedo() { PostUndo(); }
	virtual void PreEditChange(FProperty* PropertyAboutToChange) {}
	virtual void PostEditChange(const FPropertyChangedEvent& PropertyChangedEvent) {}

	using FRegisterTab = TFunctionRef<void(FName TabId, FText DisplayName, FName IconName, TSharedPtr<SWidget> Widget)>;
	virtual void RegisterTabs(FRegisterTab RegisterTab) {}

	virtual void BuildMenu(FMenuBarBuilder& MenuBarBuilder) {}
	virtual void BuildToolbar(FToolBarBuilder& ToolbarBuilder) {}
	virtual TSharedPtr<FTabManager::FLayout> GetLayout() const VOXEL_PURE_VIRTUAL({});
	virtual TSharedPtr<SWidget> GetMenuOverlay() const { return nullptr; }
	virtual void SetTabManager(const TSharedRef<FTabManager>& TabManager);
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	virtual void SaveDocuments() {}
	virtual void LoadDocuments() {}

	struct FMode
	{
		UScriptStruct* Struct = nullptr;
		TAttribute<UObject*> Object;
		FText DisplayName;
		const FSlateBrush* Icon = nullptr;
		TAttribute<bool> CanBeSelected = true;
		TFunction<void(FVoxelToolkit&)> ConfigureToolkit;
	};
	virtual TArray<FMode> GetModes() const { return {}; }
	virtual UScriptStruct* GetDefaultMode() const { return nullptr; }

public:
	static FVoxelToolkit* OpenToolkit(const UObject& Asset, const UScriptStruct* ToolkitStruct);

	template<typename T>
	static T* OpenToolkit(const UObject& Asset)
	{
		return static_cast<T*>(FVoxelToolkit::OpenToolkit(Asset, StaticStructFast<T>()));
	}

public:
	FNotifyHook* GetNotifyHook() const
	{
		return ConstCast(this);
	}
	TSharedRef<FUICommandList> GetCommands() const
	{
		return PrivateCommands.ToSharedRef();
	}

private:
	UObject** CachedAssetPtr = nullptr;
	TSharedPtr<FUICommandList> PrivateCommands;

	struct FTicker : public FVoxelTicker
	{
		FVoxelToolkit& Toolkit;

		explicit FTicker(FVoxelToolkit& Toolkit)
			: Toolkit(Toolkit)
		{
		}

		virtual void Tick() override
		{
			VOXEL_FUNCTION_COUNTER();
			Toolkit.Tick();
		}
	};
	TSharedPtr<FTicker> PrivateTicker;
	TWeakPtr<FTabManager> WeakTabManager;

	//~ Begin FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPreChange(FEditPropertyChain* PropertyAboutToChange) override;

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged) override;
	//~ End FNotifyHook Interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override
	{
		PostUndo();
	}
	virtual void PostRedo(bool bSuccess) override
	{
		PostRedo();
	}
	//~ End FEditorUndoClient Interface
};