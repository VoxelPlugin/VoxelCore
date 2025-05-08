// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInstancedStructCustomization.h"
#include "VoxelInstancedStructNodeBuilder.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Styling/SlateIconFinder.h"

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelInstancedStruct, FVoxelInstancedStructCustomization);

void FVoxelInstancedStructCustomization::CustomizeHeader(
	const TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	VOXEL_FUNCTION_COUNTER();

	StructProperty = PropertyHandle;
	RefreshDelegate = FVoxelEditorUtilities::MakeRefreshDelegate(this, CustomizationUtils);

	FCoreUObjectDelegates::OnObjectsReinstanced.Add(MakeWeakDelegateDelegate(RefreshDelegate, [RefreshDelegate = RefreshDelegate](const FCoreUObjectDelegates::FReplacementObjectMap&)
	{
		RefreshDelegate.Execute();
	}));

	if (PropertyHandle->HasMetaData("ShowOnlyInnerProperties"))
	{
		return;
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FVoxelInstancedStructCustomization::GenerateStructPicker)
			.ContentPadding(0)
			.IsEnabled(!PropertyHandle->HasMetaData("StructTypeConst"))
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage)
					.Image(this, &FVoxelInstancedStructCustomization::GetStructIcon)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FVoxelInstancedStructCustomization::GetStructName)
					.ToolTipText(this, &FVoxelInstancedStructCustomization::GetStructTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
}

void FVoxelInstancedStructCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FVoxelInstancedStructNodeBuilder> Builder = MakeShared<FVoxelInstancedStructNodeBuilder>(PropertyHandle);
	Builder->Initialize();
	ChildBuilder.AddCustomBuilder(Builder);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FText FVoxelInstancedStructCustomization::GetStructName() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSet<UScriptStruct*> Structs;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Structs.Add(InstancedStruct.GetScriptStruct());
	});

	if (Structs.Num() > 1)
	{
		return INVTEXT("Multiple Values");
	}

	if (Structs.Num() == 0)
	{
		return INVTEXT("Invalid");
	}

	const UScriptStruct* Struct = Structs.GetUniqueValue();
	if (!Struct)
	{
		return INVTEXT("None");
	}

	return Struct->GetDisplayNameText();
}

FText FVoxelInstancedStructCustomization::GetStructTooltip() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSet<UScriptStruct*> Structs;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Structs.Add(InstancedStruct.GetScriptStruct());
	});

	if (Structs.Num() > 1)
	{
		return INVTEXT("Multiple Values");
	}

	if (Structs.Num() == 0)
	{
		return INVTEXT("Invalid");
	}

	const UScriptStruct* Struct = Structs.GetUniqueValue();
	if (!Struct)
	{
		return INVTEXT("None");
	}

	return Struct->GetToolTipText();
}

const FSlateBrush* FVoxelInstancedStructCustomization::GetStructIcon() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSet<UScriptStruct*> Structs;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Structs.Add(InstancedStruct.GetScriptStruct());
	});

	if (Structs.Num() != 1 ||
		!Structs.GetUniqueValue())
	{
		return nullptr;
	}

	return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> FVoxelInstancedStructCustomization::GenerateStructPicker() const
{
	VOXEL_FUNCTION_COUNTER();

	class FInstancedStructFilter : public IStructViewerFilter
	{
	public:
		const UScriptStruct* BaseStruct = nullptr;
		bool bAllowBaseStruct = true;

		//~ Begin IStructViewerFilter Interface
		virtual bool IsStructAllowed(
			const FStructViewerInitializationOptions& InitOptions,
			const UScriptStruct* Struct,
			const TSharedRef<FStructViewerFilterFuncs> FilterFuncs) override
		{
			if (Struct->HasMetaData(TEXT("Hidden")) ||
				Struct->HasMetaData(TEXT("Abstract")))
			{
				return false;
			}

			if (!BaseStruct)
			{
				return true;
			}

			if (Struct == BaseStruct)
			{
				return bAllowBaseStruct;
			}

			return Struct->IsChildOf(BaseStruct);
		}

		virtual bool IsUnloadedStructAllowed(
			const FStructViewerInitializationOptions& InitOptions,
			const FSoftObjectPath& StructPath,
			const TSharedRef<FStructViewerFilterFuncs> FilterFuncs) override
		{
			return false;
		}
		//~ End IStructViewerFilter Interface
	};

	const TSharedRef<FInstancedStructFilter> StructFilter = MakeShared<FInstancedStructFilter>();

	StructFilter->BaseStruct = INLINE_LAMBDA -> UScriptStruct*
	{
		const FString BaseStructName = StructProperty->GetMetaData("BaseStruct");
		if (BaseStructName.IsEmpty())
		{
			return nullptr;
		}

		if (!ensure(BaseStructName.StartsWith("/Script/")))
		{
			return nullptr;
		}

		return LoadObject<UScriptStruct>(nullptr, *BaseStructName);
	};

	StructFilter->bAllowBaseStruct = !StructProperty->HasMetaData("ExcludeBaseStruct");

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = !(StructProperty->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = StructProperty->HasMetaData("ShowTreeView") ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !StructProperty->HasMetaData("HideViewOptions");

	// Just pick the first one as selected
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Options.SelectedStruct = InstancedStruct.GetScriptStruct();
	});

	FStructViewerModule& StructViewerModule = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer");

	const TSharedRef<SWidget> Viewer = StructViewerModule.CreateStructViewer(Options, MakeLambdaDelegate([this](const UScriptStruct* Struct)
	{
		ComboButton->SetIsOpen(false);

		if (!StructProperty->IsValidHandle())
		{
			return;
		}

		FScopedTransaction Transaction(INVTEXT("Set Struct"));

		StructProperty->NotifyPreChange();

		FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](FVoxelInstancedStruct& InstancedStruct)
		{
			InstancedStruct.InitializeAs(ConstCast(Struct));
		});

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		(void)RefreshDelegate.ExecuteIfBound();
	}));

	return
		SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				Viewer
			]
		];
}