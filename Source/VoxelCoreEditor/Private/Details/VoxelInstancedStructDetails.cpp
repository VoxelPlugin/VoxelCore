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

inline const UScriptStruct* GetCommonScriptStruct(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UScriptStruct* CommonStructType = nullptr;

	StructProperty->EnumerateConstRawData([&CommonStructType](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			const FVoxelInstancedStruct* InstancedStruct = static_cast<const FVoxelInstancedStruct*>(RawData);

			const UScriptStruct* StructTypePtr = InstancedStruct->GetScriptStruct();
			if (CommonStructType && CommonStructType != StructTypePtr)
			{
				// Multiple struct types on the sources - show nothing set
				CommonStructType = nullptr;
				return false;
			}
			CommonStructType = StructTypePtr;
		}

		return true;
	});

	return CommonStructType;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelInstancedStructDataDetails::FVoxelInstancedStructDataDetails(TSharedPtr<IPropertyHandle> InStructProperty)
{
	check(CastFieldChecked<FStructProperty>(InStructProperty->GetProperty())->Struct == FVoxelInstancedStruct::StaticStruct());

	StructProperty = InStructProperty;
}

void FVoxelInstancedStructDataDetails::OnStructValuePreChange()
{
	// Forward the change event to the real struct handle
	if (StructProperty && StructProperty->IsValidHandle())
	{
		StructProperty->NotifyPreChange();
	}
}

void FVoxelInstancedStructDataDetails::OnStructValuePostChange()
{
	// Forward the change event to the real struct handle
	if (StructProperty && StructProperty->IsValidHandle())
	{
		TGuardValue<bool> HandlingStructValuePostChangeGuard(bIsHandlingStructValuePostChange, true);

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}
}

void FVoxelInstancedStructDataDetails::OnStructHandlePostChange()
{
	if (!bIsHandlingStructValuePostChange)
	{
		// External change; force a sync next Tick
		LastSyncEditableInstanceFromSourceSeconds = 0.0;
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
	if (!LastStruct)
	{
		LastStruct = GetCommonScriptStruct(StructProperty);
	}

	// Create a struct instance to edit, for the common struct type of the sources being edited
	StructInstanceData = MakeVoxelShared<FVoxelInstancedStructProvider>(StructProperty);

	// Add the rows for the struct
	if (StructInstanceData)
	{
		const FSimpleDelegate OnStructValuePreChangeDelegate = FSimpleDelegate::CreateSP(this, &FVoxelInstancedStructDataDetails::OnStructValuePreChange);
		const FSimpleDelegate OnStructValuePostChangeDelegate = FSimpleDelegate::CreateSP(this, &FVoxelInstancedStructDataDetails::OnStructValuePostChange);

		TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(StructInstanceData.ToSharedRef());
		for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildProperties)
		{
			ChildHandle->SetOnPropertyValuePreChange(OnStructValuePreChangeDelegate);
			ChildHandle->SetOnChildPropertyValuePreChange(OnStructValuePreChangeDelegate);
			ChildHandle->SetOnPropertyValueChanged(OnStructValuePostChangeDelegate);
			ChildHandle->SetOnChildPropertyValueChanged(OnStructValuePostChangeDelegate);

			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

void FVoxelInstancedStructDataDetails::Tick(float DeltaTime)
{
	if (LastSyncEditableInstanceFromSourceSeconds + 0.1 < FPlatformTime::Seconds())
	{
		if (LastStruct != GetCommonScriptStruct(StructProperty))
		{
			// If the editable struct no longer has the same struct type as the underlying source,
			// then we need to refresh to update the child property rows for the new type
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

FName FVoxelInstancedStructDataDetails::GetName() const
{
	static const FName Name("InstancedStructDataDetails");
	return Name;
}

void FVoxelInstancedStructDataDetails::PostUndo(bool bSuccess)
{
	// Undo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

void FVoxelInstancedStructDataDetails::PostRedo(bool bSuccess)
{
	// Redo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FVoxelInstancedStructDetails::MakeInstance()
{
	return MakeVoxelShared<FVoxelInstancedStructDetails>();
}

void FVoxelInstancedStructDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	static const FName NAME_BaseStruct = "BaseStruct";
	static const FName NAME_StructTypeConst = "StructTypeConst";
	static const FName NAME_ShowOnlyInnerProperties = "ShowOnlyInnerProperties";

	StructProperty = StructPropertyHandle;

	const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty();

	if (StructProperty->HasMetaData(NAME_ShowOnlyInnerProperties))
	{
		return;
	}

	const bool bEnableStructSelection = !MetaDataProperty->HasMetaData(NAME_StructTypeConst);

	BaseScriptStruct = nullptr;
	{
		FString BaseStructName = MetaDataProperty->GetMetaData(NAME_BaseStruct);
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
					.ToolTipText(this, &FVoxelInstancedStructDetails::GetDisplayValueString)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

void FVoxelInstancedStructDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<FVoxelInstancedStructDataDetails> DataDetails = MakeVoxelShared<FVoxelInstancedStructDataDetails>(StructProperty);
	StructBuilder.AddCustomBuilder(DataDetails);
}

FText FVoxelInstancedStructDetails::GetDisplayValueString() const
{
	const UScriptStruct* ScriptStruct = GetCommonScriptStruct(StructProperty);
	if (ScriptStruct)
	{
		return ScriptStruct->GetDisplayNameText();
	}
	return LOCTEXT("NullScriptStruct", "None");
}

const FSlateBrush* FVoxelInstancedStructDetails::GetDisplayValueIcon() const
{
	return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
}

TSharedRef<SWidget> FVoxelInstancedStructDetails::GenerateStructPicker()
{
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";

	const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty();

	const bool bAllowNone = !(MetaDataProperty->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = MetaDataProperty->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = MetaDataProperty->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FInstancedStructFilter> StructFilter = MakeVoxelShared<FInstancedStructFilter>();
	StructFilter->BaseStruct = BaseScriptStruct;
	StructFilter->bAllowUserDefinedStructs = false;
	StructFilter->bAllowBaseStruct = false;

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;

	FOnStructPicked OnPicked(FOnStructPicked::CreateRaw(this, &FVoxelInstancedStructDetails::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, OnPicked)
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
	}

	ComboButton->SetIsOpen(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelInstancedStructProvider::IsValid() const
{
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