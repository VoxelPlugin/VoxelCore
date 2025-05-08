// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorUtilities.h"
#include "VoxelEditorMinimal.h"
#include "SceneView.h"
#include "ContentBrowserModule.h"
#include "EditorViewportClient.h"
#include "DetailGroup.h"
#include "DetailPropertyRow.h"
#include "DetailCategoryBuilderImpl.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelDetailCustomization);
DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPropertyTypeCustomizationBase);

TSet<TWeakPtr<IPropertyHandle>> GVoxelWeakPropertyHandles;

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	GOnVoxelModuleUnloaded.AddLambda([]
	{
		for (const TWeakPtr<IPropertyHandle>& WeakHandle : GVoxelWeakPropertyHandles)
		{
			// If this is raised we likely have a self-referencing handle
			ensure(!WeakHandle.IsValid());
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelRefreshDelegateTicker : public FVoxelEditorSingleton
{
public:
	TSet<TWeakPtr<IPropertyUtilities>> UtilitiesToRefresh;

	//~ Begin FVoxelEditorSingleton Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		const TSet<TWeakPtr<IPropertyUtilities>> UtilitiesToRefreshCopy = MoveTemp(UtilitiesToRefresh);
		ensure(UtilitiesToRefresh.Num() == 0);

		for (const TWeakPtr<IPropertyUtilities>& Utility : UtilitiesToRefreshCopy)
		{
			const TSharedPtr<IPropertyUtilities> Pinned = Utility.Pin();
			if (!Pinned)
			{
				continue;
			}

			Pinned->ForceRefresh();
		}
	}
	//~ End FVoxelEditorSingleton Interface
};
FVoxelRefreshDelegateTicker* GVoxelRefreshDelegateTicker = new FVoxelRefreshDelegateTicker();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString Voxel_GetCommandsName(FString Name)
{
	Name.RemoveFromStart("F");
	Name.RemoveFromEnd("Commands");
	return Name;
}

void FVoxelEditorUtilities::EnableRealtime()
{
	FViewport* Viewport = GEditor->GetActiveViewport();
	if (Viewport)
	{
		FViewportClient* Client = Viewport->GetClient();
		if (Client)
		{
			for (FEditorViewportClient* EditorViewportClient : GEditor->GetAllViewportClients())
			{
				if (EditorViewportClient == Client)
				{
					EditorViewportClient->SetRealtime(true);
					EditorViewportClient->SetShowStats(true); // Show stats as well
					break;
				}
			}
		}
	}
}

void FVoxelEditorUtilities::TrackHandle(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle ||
		GVoxelWeakPropertyHandles.Contains(PropertyHandle))
	{
		return;
	}

	VOXEL_FUNCTION_COUNTER();

	for (auto It = GVoxelWeakPropertyHandles.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	GVoxelWeakPropertyHandles.Add(PropertyHandle);
}

FSlateFontInfo FVoxelEditorUtilities::Font()
{
	// PropertyEditorConstants::PropertyFontStyle
	return FAppStyle::GetFontStyle("PropertyWindow.NormalFont");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelEditorUtilities::HideComponentProperties(const IDetailLayoutBuilder& DetailLayout)
{
	for (const FProperty& Property : GetClassProperties<UActorComponent>())
	{
		if (const TSharedPtr<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(Property.GetFName(), UActorComponent::StaticClass()))
		{
			PropertyHandle->MarkHiddenByCustomization();
		}
	}
}

void FVoxelEditorUtilities::SetSortOrder(
	IDetailLayoutBuilder& DetailLayout,
	const FName Name,
	const ECategoryPriority::Type Priority,
	const int32 PriorityOffset)
{
	static_cast<FDetailCategoryImpl&>(DetailLayout.EditCategory(Name)).SetSortOrder(int32(Priority) * 1000 + PriorityOffset);
}

void FVoxelEditorUtilities::HideAndMoveToCategory(
	IDetailLayoutBuilder& DetailLayout,
	const FName SourceCategory,
	const FName DestCategory,
	const TSet<FName>& ExplicitProperties,
	const bool bCreateGroup,
	const ECategoryPriority::Type Priority)
{
	IDetailCategoryBuilder& SourceCategoryBuilder = DetailLayout.EditCategory(SourceCategory);
	TArray<TSharedRef<IPropertyHandle>> Properties;
	SourceCategoryBuilder.GetDefaultProperties(Properties);

	IDetailCategoryBuilder& DestCategoryBuilder = DetailLayout.EditCategory(DestCategory, {}, Priority);

	DetailLayout.HideCategory(SourceCategory);

	if (!bCreateGroup)
	{
		for (const TSharedRef<IPropertyHandle>& Property : Properties)
		{
			if (ExplicitProperties.Num() == 0 ||
				ExplicitProperties.Contains(Property->GetProperty()->GetFName()))
			{
				DestCategoryBuilder.AddProperty(Property);
			}
		}
		return;
	}

	IDetailGroup& Group = DestCategoryBuilder.AddGroup(SourceCategory, SourceCategoryBuilder.GetDisplayName());
	for (const TSharedRef<IPropertyHandle>& Property : Properties)
	{
		if (ExplicitProperties.Num() == 0 ||
			ExplicitProperties.Contains(Property->GetProperty()->GetFName()))
		{
			Group.AddPropertyRow(Property);
		}
	}
}

DEFINE_PRIVATE_ACCESS(FDetailGroup, ParentCategory);

IDetailCategoryBuilder* FVoxelEditorUtilities::GetParentCategory(IDetailGroup& Group)
{
	return PrivateAccess::ParentCategory(static_cast<FDetailGroup&>(Group)).Pin().Get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const FVoxelDetailInterface& DetailInterface)
{
	if (DetailInterface.IsCategoryBuilder())
	{
		return MakeRefreshDelegate(DetailCustomization, DetailInterface.GetCategoryBuilder());
	}
	else
	{
		return MakeRefreshDelegate(DetailCustomization, DetailInterface.GetChildrenBuilder());
	}
}

DEFINE_PRIVATE_ACCESS(FDetailPropertyRow, ParentCategory);

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const IDetailsViewPrivate* DetailView = PrivateAccess::ParentCategory(static_cast<const FDetailPropertyRow&>(CustomizationUtils)).Pin()->UE_506_SWITCH(GetDetailsView, GetDetailsViewSharedPtr().Get)();
	return MakeRefreshDelegate(DetailCustomization, CustomizationUtils.GetPropertyUtilities(), DetailView);
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailLayoutBuilder& DetailLayout)
{
	return MakeRefreshDelegate(DetailCustomization, DetailLayout.GetPropertyUtilities(), DetailLayout.UE_506_SWITCH(GetDetailsView, GetDetailsViewSharedPtr().Get)());
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailCategoryBuilder& CategoryBuilder)
{
	return MakeRefreshDelegate(DetailCustomization, CategoryBuilder.GetParentLayout());
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IDetailCustomization* DetailCustomization, const IDetailChildrenBuilder& ChildrenBuilder)
{
	return MakeRefreshDelegate(DetailCustomization, ChildrenBuilder.GetParentCategory());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const FVoxelDetailInterface& DetailInterface)
{
	if (DetailInterface.IsCategoryBuilder())
	{
		return MakeRefreshDelegate(DetailCustomization, DetailInterface.GetCategoryBuilder());
	}
	else
	{
		return MakeRefreshDelegate(DetailCustomization, DetailInterface.GetChildrenBuilder());
	}
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const IDetailsViewPrivate* DetailView = PrivateAccess::ParentCategory(static_cast<const FDetailPropertyRow&>(CustomizationUtils)).Pin()->UE_506_SWITCH(GetDetailsView, GetDetailsViewSharedPtr().Get)();
	return MakeRefreshDelegate(DetailCustomization, CustomizationUtils.GetPropertyUtilities(), DetailView);
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailLayoutBuilder& DetailLayout)
{
	return MakeRefreshDelegate(DetailCustomization, DetailLayout.GetPropertyUtilities(), DetailLayout.UE_506_SWITCH(GetDetailsView, GetDetailsViewSharedPtr().Get)());
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailCategoryBuilder& CategoryBuilder)
{
	return MakeRefreshDelegate(DetailCustomization, CategoryBuilder.GetParentLayout());
}

FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const IDetailChildrenBuilder& ChildrenBuilder)
{
	return MakeRefreshDelegate(DetailCustomization, ChildrenBuilder.GetParentCategory());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FSimpleDelegate FVoxelEditorUtilities::MakeRefreshDelegate(
	T* DetailCustomization,
	const TSharedPtr<IPropertyUtilities>& PropertyUtilities,
	const IDetailsView* DetailsView)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(DetailCustomization) ||
		!ensure(PropertyUtilities) ||
		!ensure(DetailsView))
	{
		return {};
	}

	return MakeWeakPtrDelegate(DetailCustomization, [WeakUtilities = MakeWeakPtr(PropertyUtilities), WeakDetailView = MakeWeakPtr(DetailsView)]()
	{
		// If this is raised the property handle outlived the utilities
		ensure(WeakUtilities.IsValid());

		if (!WeakDetailView.IsValid())
		{
			return;
		}

		// Delay call to avoid all kind of issues with doing the refresh immediately
		GVoxelRefreshDelegateTicker->UtilitiesToRefresh.Add(WeakUtilities);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelEditorUtilities::RegisterAssetContextMenu(UClass* Class, const FText& Label, const FText& ToolTip, TFunction<void(UObject*)> Lambda)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda([=](const TArray<FAssetData>& SelectedAssets)
	{
		const TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		for (const FAssetData& Asset : SelectedAssets)
		{
			if (!Asset.GetClass()->IsChildOf(Class))
			{
				return Extender;
			}
		}

		Extender->AddMenuExtension(
			"CommonAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateLambda([=](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
				Label,
				ToolTip,
				FSlateIcon(NAME_None, NAME_None),
				FUIAction(FExecuteAction::CreateLambda([=]
				{
					for (const FAssetData& Asset : SelectedAssets)
					{
						UObject* Object = Asset.GetAsset();
						if (ensure(Object) && ensure(Object->IsA(Class)))
						{
							Lambda(Object);
						}
					}
				})));
			}));

		return Extender;
	}));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TArray<FName> FVoxelEditorUtilities::GetPropertyOptions(const TSharedRef<IPropertyHandle>& PropertyHandle)
{
	TArray<TSharedPtr<FString>> SharedOptions;
	TArray<FText> Tooltips;
	TArray<bool> RestrictedItems;
	PropertyHandle->GeneratePossibleValues(SharedOptions, Tooltips, RestrictedItems);

	TArray<FName> Options;
	for (const TSharedPtr<FString>& Option : SharedOptions)
	{
		Options.Add(**Option);
	}
	return Options;
}

TArray<TSharedRef<IPropertyHandle>> FVoxelEditorUtilities::GetChildHandles(
	const TSharedPtr<IPropertyHandle>& PropertyHandle,
	const bool bRecursive,
	const bool bIncludeSelf)
{
	TrackHandle(PropertyHandle);

	TArray<TSharedRef<IPropertyHandle>> ChildHandles;

	const TFunction<void(const TSharedPtr<IPropertyHandle>&)> AddHandle = [&](const TSharedPtr<IPropertyHandle>& Handle)
	{
		TrackHandle(Handle);

		if (!ensure(Handle))
		{
			return;
		}

		if (bIncludeSelf ||
			Handle != PropertyHandle)
		{
			ChildHandles.Add(Handle.ToSharedRef());
		}

		if (!bRecursive &&
			Handle != PropertyHandle)
		{
			return;
		}

		uint32 NumChildren = 0;
		if (!ensure(Handle->GetNumChildren(NumChildren) == FPropertyAccess::Success))
		{
			return;
		}

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			AddHandle(Handle->GetChildHandle(ChildIndex));
		}
	};

	AddHandle(PropertyHandle);

	return ChildHandles;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<IPropertyHandle> FVoxelEditorUtilities::MakePropertyHandle(
	const FVoxelDetailInterface& DetailInterface,
	const TArray<UObject*>& Objects,
	const FName PropertyName)
{
	IDetailPropertyRow* Row = DetailInterface.AddExternalObjectProperty(
		Objects,
		PropertyName,
		FAddPropertyParams().ForceShowProperty());

	if (!ensure(Row))
	{
		return nullptr;
	}

	Row->Visibility(EVisibility::Collapsed);

	return Row->GetPropertyHandle();
}

TSharedPtr<IPropertyHandle> FVoxelEditorUtilities::MakePropertyHandle(
	IDetailLayoutBuilder& DetailLayout,
	const TArray<UObject*>& Objects,
	const FName PropertyName)
{
	return MakePropertyHandle(
		DetailLayout.EditCategory(""),
		Objects,
		PropertyName);
}

TSharedPtr<IPropertyHandle> FVoxelEditorUtilities::MakePropertyHandle(
	IDetailLayoutBuilder& DetailLayout,
	UObject* Object,
	const FName PropertyName)
{
	return MakePropertyHandle(DetailLayout, TArray<UObject*>{ Object }, PropertyName);
}

TSharedPtr<IPropertyHandle> FVoxelEditorUtilities::MakePropertyHandle(
	IDetailLayoutBuilder& DetailLayout,
	const FName PropertyName)
{
	TArray<TWeakObjectPtr<UObject>> WeakObjects;
	DetailLayout.GetObjectsBeingCustomized(WeakObjects);

	TArray<UObject*> Objects;
	Objects.Reserve(WeakObjects.Num());

	for (const TWeakObjectPtr<UObject>& WeakObject : WeakObjects)
	{
		Objects.Add(WeakObject.Get());
	}

	return MakePropertyHandle(DetailLayout, Objects, PropertyName);
}

TSharedPtr<IPropertyHandle> FVoxelEditorUtilities::FindMapValuePropertyHandle(
	const IPropertyHandle& MapPropertyHandle,
	const FGuid& Guid)
{

	uint32 Count = 0;
	MapPropertyHandle.GetNumChildren(Count);

	TSharedPtr<IPropertyHandle> PropertyHandle;
	for (uint32 Index = 0; Index < Count; Index++)
	{
		const TSharedPtr<IPropertyHandle> ChildPropertyHandle = MapPropertyHandle.GetChildHandle(Index);
		if (!ensure(ChildPropertyHandle))
		{
			continue;
		}

		const TSharedPtr<IPropertyHandle> KeyHandle = ChildPropertyHandle->GetKeyHandle();
		if (!ensure(KeyHandle))
		{
			continue;
		}

		if (GetStructPropertyValue<FGuid>(KeyHandle) != Guid)
		{
			continue;
		}

		ensure(!PropertyHandle);
		PropertyHandle = ChildPropertyHandle;
	}

	return PropertyHandle;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelEditorUtilities::RegisterClassLayout(const UClass* Class, FOnGetDetailCustomizationInstance Delegate)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FName Name = Class->GetFName();
	ensure(!PropertyModule.GetClassNameToDetailLayoutNameMap().Contains(Name));

	PropertyModule.RegisterCustomClassLayout(Name, Delegate);
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FVoxelEditorUtilities::RegisterStructLayout(const UScriptStruct* Struct, FOnGetPropertyTypeCustomizationInstance Delegate, const bool bRecursive)
{
	RegisterStructLayout(Struct, Delegate, bRecursive, nullptr);
}

void FVoxelEditorUtilities::RegisterStructLayout(const UScriptStruct* Struct, FOnGetPropertyTypeCustomizationInstance Delegate, const bool bRecursive, const TSharedPtr<IPropertyTypeIdentifier>& Identifier)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(Struct->GetFName(), Delegate, Identifier);

	if (bRecursive)
	{
		for (const UScriptStruct* ChildStruct : GetDerivedStructs(Struct))
		{
			PropertyModule.RegisterCustomPropertyTypeLayout(ChildStruct->GetFName(), Delegate, Identifier);
		}
	}
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FVoxelEditorUtilities::RegisterEnumLayout(const UEnum* Enum, FOnGetPropertyTypeCustomizationInstance Delegate, const TSharedPtr<IPropertyTypeIdentifier>& Identifier)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(Enum->GetFName(), Delegate, Identifier);
	PropertyModule.NotifyCustomizationModuleChanged();
}

bool FVoxelEditorUtilities::GetRayInfo(FEditorViewportClient* ViewportClient, FVector& OutStart, FVector& OutEnd)
{
	if (!ensure(ViewportClient))
	{
		return false;
	}

	const FViewport* Viewport = ViewportClient->Viewport;
	if (!ensure(Viewport))
	{
		return false;
	}

	const int32 MouseX = Viewport->GetMouseX();
	const int32 MouseY = Viewport->GetMouseY();

	FSceneViewFamilyContext ViewFamily(FSceneViewFamilyContext::ConstructionValues(Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));

	const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);

	const FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);
	const FVector MouseViewportRayDirection = MouseViewportRay.GetDirection();

	OutStart = MouseViewportRay.GetOrigin();
	OutEnd = OutStart + WORLD_MAX * MouseViewportRayDirection;

	if (ViewportClient->IsOrtho())
	{
		OutStart -= WORLD_MAX * MouseViewportRayDirection;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelThumbnailTicker : public FVoxelEditorSingleton
{
public:
	TSharedPtr<FAssetThumbnailPool> Pool;

	//~ Begin FVoxelEditorSingleton Interface
	virtual void Initialize() override
	{
		Pool = MakeShared<FAssetThumbnailPool>(48);

		FCoreDelegates::OnPreExit.AddLambda([this]
		{
			// Unsafe to destroy if UObjectInitialized is false
			check(UObjectInitialized());
			Pool.Reset();
		});
	}
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		Pool->Tick(FApp::GetDeltaTime());
	}
	//~ End FVoxelEditorSingleton Interface
};
FVoxelThumbnailTicker* GVoxelThumbnailTicker = new FVoxelThumbnailTicker();

TSharedRef<FAssetThumbnailPool> FVoxelEditorUtilities::GetThumbnailPool()
{
	return GVoxelThumbnailTicker->Pool.ToSharedRef();
}