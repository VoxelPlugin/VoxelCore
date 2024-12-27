// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInstancedStructDetails.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Engine/UserDefinedStruct.h"
#include "Styling/SlateIconFinder.h"
#if VOXEL_ENGINE_VERSION >= 505
#include "StructUtils/UserDefinedStruct.h"
#endif

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelInstancedStruct, FVoxelInstancedStructDetails);

class FVoxelPropertyTypeCustomizationUtils : public IPropertyTypeCustomizationUtils
{
public:
	FVoxelPropertyTypeCustomizationUtils(const IPropertyTypeCustomizationUtils& Utils)
		: ThumbnailPool(Utils.GetThumbnailPool())
		, PropertyUtilities(Utils.GetPropertyUtilities())
	{}

	//~ Begin IPropertyTypeCustomizationUtils Interface
	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override
	{
		return ThumbnailPool;
	}
	virtual TSharedPtr<IPropertyUtilities> GetPropertyUtilities() const override
	{
		return PropertyUtilities;
	}
	//~ End IPropertyTypeCustomizationUtils Interface

private:
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};

class FInstancedStructFilter : public IStructViewerFilter
{
public:
	// The base struct for the property that classes must be a child-of
	const UScriptStruct* BaseStruct = nullptr;

	// A flag controlling whether we allow UserDefinedStructs
	bool bAllowUserDefinedStructs = false;

	// A flag controlling whether we allow to select the BaseStruct
	bool bAllowBaseStruct = true;

	virtual bool IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		if (InStruct->IsA<UUserDefinedStruct>())
		{
			return bAllowUserDefinedStructs;
		}

		if (InStruct == BaseStruct)
		{
			return bAllowBaseStruct;
		}

		if (InStruct->HasMetaData(TEXT("Hidden")))
		{
			return false;
		}

		// Query the native struct to see if it has the correct parent type (if any)
		return !BaseStruct || InStruct->IsChildOf(BaseStruct);
	}

	virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		// User Defined Structs don't support inheritance, so only include them requested
		return bAllowUserDefinedStructs;
	}
};

inline FPropertyAccess::Result GetCommonScriptStruct(const TSharedPtr<IPropertyHandle>& StructProperty, const UScriptStruct*& OutCommonStruct)
{
	bool bHasResult = false;
	bool bHasMultipleValues = false;

	StructProperty->EnumerateConstRawData([&OutCommonStruct, &bHasResult, &bHasMultipleValues](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (const FVoxelInstancedStruct* InstancedStruct = static_cast<const FVoxelInstancedStruct*>(RawData))
		{
			const UScriptStruct* Struct = InstancedStruct->GetScriptStruct();

			if (!bHasResult)
			{
				OutCommonStruct = Struct;
			}
			else if (OutCommonStruct != Struct)
			{
				bHasMultipleValues = true;
			}

			bHasResult = true;
		}

		return true;
	});

	if (bHasMultipleValues)
	{
		return FPropertyAccess::MultipleValues;
	}

	return bHasResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInstancedStructDataDetails::FVoxelInstancedStructDataDetails(
	const TSharedPtr<IPropertyHandle>& InStructProperty,
	const TSharedRef<FVoxelInstancedStructProvider>& StructProvider,
	const IPropertyTypeCustomizationUtils& PropertyUtils)
	: StructProvider(StructProvider)
	, PropertyUtils(MakeShared<FVoxelPropertyTypeCustomizationUtils>(PropertyUtils))
{
	check(CastFieldChecked<FStructProperty>(InStructProperty->GetProperty())->Struct == FVoxelInstancedStruct::StaticStruct());

	StructProperty = InStructProperty;
}

void FVoxelInstancedStructDataDetails::OnStructHandlePostChange()
{
	if (StructProvider.IsValid())
	{
		TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
		if (InstanceTypes != CachedInstanceTypes)
		{
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

TArray<TWeakObjectPtr<const UStruct>> FVoxelInstancedStructDataDetails::GetInstanceTypes() const
{
	TArray<TWeakObjectPtr<const UStruct>> Result;

	StructProperty->EnumerateConstRawData([&Result](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		TWeakObjectPtr<const UStruct>& Type = Result.AddDefaulted_GetRef();
		if (const FVoxelInstancedStruct* InstancedStruct = static_cast<const FVoxelInstancedStruct*>(RawData))
		{
			Result.Add(InstancedStruct->GetScriptStruct());
		}
		else
		{
			Result.Add(nullptr);
		}
		return true;
	});

	return Result;
}

void FVoxelInstancedStructDataDetails::OnStructLayoutChanges()
{
	if (StructProvider.IsValid())
	{
		const TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
		if (InstanceTypes != CachedInstanceTypes)
		{
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

void FVoxelInstancedStructDataDetails::SetOnRebuildChildren(const FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FVoxelInstancedStructDataDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	StructProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FVoxelInstancedStructDataDetails::OnStructHandlePostChange));
}

void FVoxelInstancedStructDataDetails::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	if (StructProperty->GetNumPerObjectValues() > 500 &&
		!bDisableObjectCountLimit)
	{
		ChildBuilder.AddCustomRow(INVTEXT("Expand"))
		.WholeRowContent()
		[
			SNew(SVoxelDetailButton)
			.Text(FText::FromString(FString::Printf(TEXT("Expand %d structs"), StructProperty->GetNumPerObjectValues())))
			.OnClicked_Lambda([this]
			{
				bDisableObjectCountLimit = true;
				OnRegenerateChildren.ExecuteIfBound();
				return FReply::Handled();
			})
		];

		CachedInstanceTypes = GetInstanceTypes();
		return;
	}

	TArray<TSharedPtr<IPropertyHandle>> ChildProperties;
	TSharedPtr<IPropertyHandle> RootHandle;
	if (bIsInitialGeneration)
	{
		RootHandle = StructProperty->GetChildHandle(0);

		uint32 NumChildren = 0;
		RootHandle->GetNumChildren(NumChildren);

		FVoxelUtilities::SetNum(ChildProperties, NumChildren);

		for (uint32 Index = 0; Index < NumChildren; Index++)
		{
			ChildProperties[Index] = RootHandle->GetChildHandle(Index);
		}

		bIsInitialGeneration = false;
	}
	else
	{
		const TSharedRef<FVoxelInstancedStructProvider> NewStructProvider = MakeShared<FVoxelInstancedStructProvider>(StructProperty);
		StructProvider = NewStructProvider;
		ChildProperties = StructProperty->AddChildStructure(NewStructProvider);

		uint32 NumChildren = 0;
		StructProperty->GetNumChildren(NumChildren);

		if (ensure(NumChildren > 0))
		{
			RootHandle = StructProperty->GetChildHandle(NumChildren - 1);
		}
	}

	// Pass metadata to newly constructed handle
	if (RootHandle &&
		RootHandle->IsValidHandle())
	{
		if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
		{
			if (const TMap<FName, FString>* MetaDataMap = MetaDataProperty->GetMetaDataMap())
			{
				for (const auto& It : *MetaDataMap)
				{
					RootHandle->SetInstanceMetaData(It.Key, It.Value);
				}
			}
		}

		if (const TMap<FName, FString>* MetaDataMap = StructProperty->GetInstanceMetaDataMap())
		{
			for (const auto& It : *MetaDataMap)
			{
				RootHandle->SetInstanceMetaData(It.Key, It.Value);
			}
		}
	}

	CachedInstanceTypes = GetInstanceTypes();

	const UScriptStruct* ActiveStruct = nullptr;
	TSet<const UScriptStruct*> InstanceTypes;
	for (const TWeakObjectPtr<const UStruct>& WeakStruct : CachedInstanceTypes)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(WeakStruct.Get()))
		{
			ActiveStruct = Struct;
			InstanceTypes.Add(Struct);
		}
	}

	// Allow customization if showing only one type of struct
	if (InstanceTypes.Num() == 1 &&
		RootHandle)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		if (PropertyEditor.IsCustomizedStruct(ActiveStruct, {}))
		{
			const FPropertyTypeLayoutCallback Callback = PropertyEditor.FindPropertyTypeLayoutCallback(ActiveStruct->GetFName(), *StructProperty, {});
			if (Callback.IsValid())
			{
				const TSharedRef<IPropertyTypeCustomization> Customization = Callback.GetCustomizationInstance();
				Customization->CustomizeChildren(RootHandle.ToSharedRef(), ChildBuilder, *PropertyUtils);
				return;
			}
		}
	}

	TArray<TSharedPtr<IPropertyHandle>> AdvancedProperties;
	for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildProperties)
	{
		if (ChildHandle->GetProperty()->HasAnyPropertyFlags(CPF_AdvancedDisplay))
		{
			AdvancedProperties.Add(ChildHandle);
			continue;
		}

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}

	// All properties are advanced
	if (AdvancedProperties.Num() == ChildProperties.Num())
	{
		for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildProperties)
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
	else if (AdvancedProperties.Num() > 0)
	{
		IDetailGroup& AdvancedGroup = ChildBuilder.AddGroup("Advanced", INVTEXT("Advanced"));
		for (const TSharedPtr<IPropertyHandle>& ChildHandle : AdvancedProperties)
		{
			AdvancedGroup.AddPropertyRow(ChildHandle.ToSharedRef());
		}
	}
}

void FVoxelInstancedStructDataDetails::Tick(float DeltaTime)
{
	// If the instance types change (e.g. due to selecting new struct type), we'll need to update the layout.
	TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
	if (InstanceTypes != CachedInstanceTypes)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FVoxelInstancedStructDataDetails::GetName() const
{
	static const FName Name("InstancedStructDataDetails");
	return Name;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FVoxelInstancedStructDetails::MakeInstance()
{
	return MakeShared<FVoxelInstancedStructDetails>();
}

void FVoxelInstancedStructDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	static const FName NAME_BaseStruct = "BaseStruct";
	static const FName NAME_StructTypeConst = "StructTypeConst";
	static const FName NAME_ShowOnlyInnerProperties = "ShowOnlyInnerProperties";

	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddSP(this, &FVoxelInstancedStructDetails::OnObjectsReinstanced);

	if (StructPropertyHandle->HasMetaData(NAME_ShowOnlyInnerProperties))
	{
		return;
	}

	const bool bEnableStructSelection = !StructPropertyHandle->HasMetaData(NAME_StructTypeConst);

	BaseScriptStruct = nullptr;
	{
		FString BaseStructName = StructPropertyHandle->GetMetaData(NAME_BaseStruct);
		if (!BaseStructName.IsEmpty())
		{
			BaseScriptStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
			if (!BaseScriptStruct)
			{
				BaseScriptStruct = LoadObject<UScriptStruct>(nullptr, *BaseStructName);
			}
			if (!BaseScriptStruct)
			{
				BaseStructName.RemoveFromStart("F");
				BaseScriptStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
			}
		}
	}

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FVoxelInstancedStructDetails::GenerateStructPicker)
			.ContentPadding(0)
			.IsEnabled(bEnableStructSelection)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage)
					.Image(this, &FVoxelInstancedStructDetails::GetDisplayValueIcon)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FVoxelInstancedStructDetails::GetDisplayValueString)
					.ToolTipText(this, &FVoxelInstancedStructDetails::GetTooltipText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

void FVoxelInstancedStructDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const TSharedRef<FVoxelInstancedStructProvider> NewStructProvider = MakeShared<FVoxelInstancedStructProvider>(StructProperty);
	const TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(NewStructProvider);

	if (ChildProperties.Num() == 0)
	{
		return;
	}

	const TSharedRef<FVoxelInstancedStructDataDetails> DataDetails = MakeShared<FVoxelInstancedStructDataDetails>(StructProperty, NewStructProvider, StructCustomizationUtils);
	StructBuilder.AddCustomBuilder(DataDetails);
}

void FVoxelInstancedStructDetails::OnObjectsReinstanced(const TMap<UObject*, UObject*>& ObjectMap)
{
	// Force update the details when BP is compiled, since we may cached hold references to the old object or class.
	if (!ObjectMap.IsEmpty() && PropUtils.IsValid())
	{
		PropUtils->RequestRefresh();
	}
}

FText FVoxelInstancedStructDetails::GetDisplayValueString() const
{
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetCommonScriptStruct(StructProperty, CommonStruct);

	switch (Result)
	{
	case FPropertyAccess::MultipleValues:
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	case FPropertyAccess::Success:
	{
		if (CommonStruct)
		{
			return CommonStruct->GetDisplayNameText();
		}
		return LOCTEXT("NullScriptStruct", "None");
	}
	default:
	{
		return {};
	}
	}
}

FText FVoxelInstancedStructDetails::GetTooltipText() const
{
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetCommonScriptStruct(StructProperty, CommonStruct);
	if (CommonStruct &&
		Result == FPropertyAccess::Success)
	{
		return CommonStruct->GetToolTipText();
	}

	return GetDisplayValueString();
}

const FSlateBrush* FVoxelInstancedStructDetails::GetDisplayValueIcon() const
{
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetCommonScriptStruct(StructProperty, CommonStruct);
	if (Result == FPropertyAccess::Success)
	{
		return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
	}

	return nullptr;
}

TSharedRef<SWidget> FVoxelInstancedStructDetails::GenerateStructPicker()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";

	const bool bExcludeBaseStruct = StructProperty->HasMetaData(NAME_ExcludeBaseStruct);
	const bool bAllowNone = !(StructProperty->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = StructProperty->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = StructProperty->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FInstancedStructFilter> StructFilter = MakeShared<FInstancedStructFilter>();
	StructFilter->BaseStruct = BaseScriptStruct;
	StructFilter->bAllowUserDefinedStructs = BaseScriptStruct == nullptr;
	StructFilter->bAllowBaseStruct = !bExcludeBaseStruct;

	const UScriptStruct* SelectedStruct = nullptr;
	GetCommonScriptStruct(StructProperty, SelectedStruct);

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;
	Options.SelectedStruct = SelectedStruct;

	FOnStructPicked OnPicked(FOnStructPicked::CreateRaw(this, &FVoxelInstancedStructDetails::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer")
				.CreateStructViewer(Options, OnPicked)
			]
		];
}

void FVoxelInstancedStructDetails::OnStructPicked(const UScriptStruct* InStruct)
{
	if (StructProperty && StructProperty->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		StructProperty->NotifyPreChange();

		FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [InStruct](FVoxelInstancedStruct& InstancedStruct)
		{
			InstancedStruct.InitializeAs(ConstCast(InStruct));
		});

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelInstancedStructProvider::IsValid() const
{
	VOXEL_FUNCTION_COUNTER();

	bool bHasValidData = false;
	EnumerateInstances([&](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
	{
		if (ScriptStruct && Memory)
		{
			bHasValidData = true;
			return false; // Stop
		}
		return true; // Continue
	});

	return bHasValidData;
}

const UStruct* FVoxelInstancedStructProvider::GetBaseStructure() const
{
	// Taken from UClass::FindCommonBase
	auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
	{
		const UScriptStruct* CommonBaseStruct = StructA;
		while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
		{
			CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
		}
		return CommonBaseStruct;
	};

	const UScriptStruct* CommonStruct = nullptr;
	EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
	{
		if (ScriptStruct)
		{
			CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
		}
		return true; // Continue
	});

	return CommonStruct;
}

void FVoxelInstancedStructProvider::GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const
{
	VOXEL_FUNCTION_COUNTER();

	// The returned instances need to be compatible with base structure.
	// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
	EnumerateInstances([&OutInstances, ExpectedBaseStructure](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
	{
		TSharedPtr<FStructOnScope> Result;

		if (ExpectedBaseStructure && ScriptStruct && ScriptStruct->IsChildOf(ExpectedBaseStructure))
		{
			Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
			Result->SetPackage(Package);
		}

		OutInstances.Add(Result);

		return true; // Continue
	});
}

uint8* FVoxelInstancedStructProvider::GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const
{
	if (!ParentValueAddress)
	{
		return nullptr;
	}

	FVoxelInstancedStruct& InstancedStruct = *reinterpret_cast<FVoxelInstancedStruct*>(ParentValueAddress);
	if (ExpectedBaseStructure &&
		InstancedStruct.GetScriptStruct() &&
		InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
	{
		return static_cast<uint8*>(InstancedStruct.GetStructMemory());
	}

	return nullptr;
}

void FVoxelInstancedStructProvider::EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!StructProperty.IsValid())
	{
		return;
	}

	TArray<UPackage*> Packages;
	StructProperty->GetOuterPackages(Packages);

	StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
	{
		const UScriptStruct* ScriptStruct = nullptr;
		uint8* Memory = nullptr;
		UPackage* Package = nullptr;
		if (FVoxelInstancedStruct* InstancedStruct = static_cast<FVoxelInstancedStruct*>(RawData))
		{
			ScriptStruct = InstancedStruct->GetScriptStruct();
			Memory = static_cast<uint8*>(InstancedStruct->GetStructMemory());

			if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
			{
				Package = Packages[DataIndex];
			}
		}

		return InFunc(ScriptStruct, Memory, Package);
	});
}

#undef LOCTEXT_NAMESPACE