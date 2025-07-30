// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "EditorDirectories.h"
#include "DesktopPlatformModule.h"
#include "ISettingsEditorModule.h"
#include "VoxelShaderHooksManager.h"
#include "Framework/Text/SlateTextRun.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

VOXEL_INITIALIZE_STYLE(VoxelShaderHooksStyle)
{
	const FTextBlockStyle DefaultText = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Mono", 8))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2f::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetHighlightShape(CORE_BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f / 8.f))
	);

	Set("DiffText_Normal", DefaultText);
	Set("DiffText_AddLine", FTextBlockStyle(DefaultText).SetColorAndOpacity(FStyleColors::AccentGreen));
	Set("DiffText_RemoveLine", FTextBlockStyle(DefaultText).SetColorAndOpacity(FStyleColors::AccentRed));
	Set("DiffText_Meta1", FTextBlockStyle(DefaultText).SetColorAndOpacity(FStyleColors::AccentBlue));
	Set("DiffText_Meta2", FTextBlockStyle(DefaultText).SetColorAndOpacity(FStyleColors::AccentPurple));
	Set("DiffText_Meta3", FTextBlockStyle(DefaultText).SetColorAndOpacity(FStyleColors::AccentOrange));
}

class FVoxelTextStyleDecorator : public ITextDecorator
{
public:
	static TSharedRef<FVoxelTextStyleDecorator> Create()
	{
		return MakeShared<FVoxelTextStyleDecorator>();
	}

	//~ Begin ITextDecorator Interface
	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
	{
		return (RunParseResult.Name == TEXT("TextStyle"));
	}

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override
	{
		FRunInfo RunInfo(RunParseResult.Name);
		for(const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
		{
			RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
		}

		FTextRange ModelRange;
		ModelRange.BeginIndex = InOutModelText->Len();
		*InOutModelText += OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex);
		ModelRange.EndIndex = InOutModelText->Len();

		return FSlateTextRun::Create(RunInfo, InOutModelText, GetTextStyle(RunInfo), ModelRange);
	}
	//~ End ITextDecorator Interface

	static const FTextBlockStyle& GetTextStyle(const FRunInfo& RunInfo)
	{
		const FString* TextStyleName = RunInfo.MetaData.Find("Style");
		if (!TextStyleName)
		{
			return FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
		}

		const ISlateStyle* StyleSet = &FAppStyle::Get();
		if (const FString* StyleSetName = RunInfo.MetaData.Find("StyleSet"))
		{
			if (const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(FName(*StyleSetName)))
			{
				StyleSet = Style;
			}
		}

		return StyleSet->GetWidgetStyle<FTextBlockStyle>(FName(*TextStyleName));
	}
};

class SVoxelShaderHookPatchPopup : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		FArguments()
		{
			_Hook = nullptr;
		}

		SLATE_ARGUMENT(FVoxelShaderHookGroup*, Hook)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	};

	void Construct(const FArguments& Args)
	{
		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10.f, 10.f, 10.f, 5.f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "Profiler.CaptionBold")
					.Text(FText::FromString("Failed to automatically apply patch"))
					.ColorAndOpacity(FStyleColors::Warning)
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.f, 0.f, 5.f, 5.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(INVTEXT("Copy to Clipboard"))
						.OnClicked_Lambda([Hook = Args._Hook]
						{
							FPlatformApplicationMisc::ClipboardCopy(*Hook->CreatePatch(false));
							VOXEL_MESSAGE(Info, "Diff was copied to clipboard");
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(INVTEXT("Save to File"))
						.OnClicked_Lambda([Hook = Args._Hook]
						{
							IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
							if (!DesktopPlatform)
							{
								return FReply::Handled();
							}

							FString FileName = Hook->DisplayName;
							FileName.ReplaceInline(TEXT(" "), TEXT("_"));
							FileName.ReplaceInline(TEXT("-"), TEXT("_"));

							TArray<FString> SaveFilenames;
							if (!DesktopPlatform->SaveFileDialog(
								FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
								"Save: " + Hook->DisplayName,
								FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
								FileName + ".diff",
								"Diff (*.diff)|*.diff",
								EFileDialogFlags::None,
								SaveFilenames))
							{
								return FReply::Handled();
							}

							if (SaveFilenames.Num() > 0)
							{
								const FString WritePath = FPaths::ConvertRelativePathToFull(SaveFilenames[0]);
								if (FFileHelper::SaveStringToFile(Hook->CreatePatch(false), *WritePath))
								{
									FPlatformProcess::ExploreFolder(*FPaths::GetPath(WritePath));
									VOXEL_MESSAGE(Info, "Diff saved to file {0}", WritePath);
								}
								else
								{
									VOXEL_MESSAGE(Error, "Failed to save diff to file");
								}
							}
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SNew(SMultiLineEditableTextBox)
					.AllowMultiLine(true)
					.IsReadOnly(true)
					.Text(FText::FromString(Args._Hook->CreatePatch(true)))
					.Marshaller(FRichTextLayoutMarshaller::Create({ FVoxelTextStyleDecorator::Create() }, &FAppStyle::Get()))
					.AlwaysShowScrollbars(true)
				]
			]
		];
	}

public:
	static void ShowDialog(FVoxelShaderHookGroup* Hook)
	{
		const TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(FText::FromString(Hook->DisplayName + " changes"))
			.SizingRule(ESizingRule::UserSized)
			.AutoCenter(EAutoCenter::PrimaryWorkArea)
			.ClientSize(FVector2D(768, 720));

		Window->SetContent
		(
			SNew(SVoxelShaderHookPatchPopup)
			.Hook(Hook)
			.WidgetWindow(Window)
		);

		const IMainFrameModule* MainFrame = FModuleManager::LoadModulePtr<IMainFrameModule>("MainFrame");
		const TSharedPtr<SWindow> ParentWindow = MainFrame != nullptr ? MainFrame->GetParentWindow() : nullptr;

		FSlateApplication::Get().AddModalWindow(
			Window,
			MainFrame != nullptr ? MainFrame->GetParentWindow() : nullptr,
			false);
	}
};

class SVoxelShaderHookRow : public SMultiColumnTableRow<TSharedPtr<FVoxelShaderHookGroup>>
{
public:
	VOXEL_SLATE_ARGS()
	{
		FArguments()
		{
			_Hook = nullptr;
		}

		SLATE_ARGUMENT(FVoxelShaderHookGroup*, Hook)
	};

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		Hook = InArgs._Hook;
		SMultiColumnTableRow<TSharedPtr<FVoxelShaderHookGroup>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == "NeverApply")
		{
			return
				SNew(SCheckBox)
				.IsChecked_Lambda([this]
				{
					return GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(Hook->StructName) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
				})
				.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState)
				{
					UVoxelShaderHooksSettings* Settings = GetMutableDefault<UVoxelShaderHooksSettings>();

					if (NewState == ECheckBoxState::Checked)
					{
						Settings->DisabledHooks.Remove(Hook->StructName);
					}
					else
					{
						Settings->DisabledHooks.Add(Hook->StructName);
					}

					Settings->PostEditChange();

					if (NewState == ECheckBoxState::Unchecked)
					{
						if (Hook->Revert())
						{
							FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor").OnApplicationRestartRequired();
						}
						else
						{
							VOXEL_MESSAGE(Error, "Failed to revert");
						}
					}

					Hook->Invalidate();
				})
				.ToolTipText_Lambda([this]
				{
					if (GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(Hook->StructName))
					{
						return INVTEXT("Check to enable this hook");
					}
					else
					{
						return INVTEXT("Uncheck to fully disable hook & revert changes");
					}
				});
		}

		if (ColumnName == "Name")
		{
			return
				SNew(SBox)
				.VAlign(VAlign_Center)
				.Padding(4.f)
				[
					SNew(STextBlock)
					.IsEnabled_Lambda([this]
					{
						return !GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(Hook->StructName);
					})
					.Text(FText::FromString(Hook->DisplayName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		if (ColumnName == "Description")
		{
			return
				SNew(SBox)
				.VAlign(VAlign_Center)
				.Padding(4.f)
				[
					SNew(STextBlock)
					.IsEnabled_Lambda([this]
					{
						return !GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(Hook->StructName);
					})
					.Text(FText::FromString(Hook->Description))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FSlateColor::UseForeground())
					.AutoWrapText(true)
				];
		}

		if (ColumnName == "State")
		{
			return
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						switch (Hook->GetState())
						{
						case EVoxelShaderHookState::NeverApply: return INVTEXT("Disabled");
						case EVoxelShaderHookState::Active: return INVTEXT("Up to date");
						case EVoxelShaderHookState::Outdated: return INVTEXT("Outdated");
						case EVoxelShaderHookState::NotApplied: return INVTEXT("Not Applied");
						case EVoxelShaderHookState::Invalid: return INVTEXT("Invalid");
						case EVoxelShaderHookState::Deprecated: return INVTEXT("Deprecated");
						default: ensure(false); return INVTEXT("None");
						}
					})
					.ToolTipText_Lambda([this]
					{
						switch (Hook->GetState())
						{
						case EVoxelShaderHookState::NeverApply: return INVTEXT("Hook is manually disabled");
						case EVoxelShaderHookState::Active: return INVTEXT("Hook is applied & up to date");
						case EVoxelShaderHookState::Outdated: return INVTEXT("Hook is outdated: a hook was applied in the past but a new one is available");
						case EVoxelShaderHookState::NotApplied: return INVTEXT("Hook is not applied, features requiring this won't work");
						case EVoxelShaderHookState::Invalid: return INVTEXT("Hook cannot be applied automatically, you will need to apply the changes manually");
						case EVoxelShaderHookState::Deprecated: return INVTEXT("Hook is deprecated and can be safely removed");
						default: ensure(false); return INVTEXT("");
						}
					})
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity_Lambda([this]
					{
						switch (Hook->GetState())
						{
						case EVoxelShaderHookState::NeverApply: return FSlateColor::UseSubduedForeground();
						case EVoxelShaderHookState::Active: return FStyleColors::Success;
						case EVoxelShaderHookState::Outdated: return FStyleColors::Warning;
						case EVoxelShaderHookState::NotApplied: return FStyleColors::Warning;
						case EVoxelShaderHookState::Invalid: return FStyleColors::Error;
						case EVoxelShaderHookState::Deprecated: return FSlateColor::UseSubduedForeground();
						default: ensure(false); return FStyleColors::Error;
						}
					})
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(22.0f)
					.HeightOverride(22.0f)
					[
						SNew(SButton)
						.IsEnabled_Lambda([this]
						{
							return !GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(Hook->StructName);
						})
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ToolTipText(INVTEXT("Reload shader from disk & check hook status"))
						.OnClicked_Lambda([this]
						{
							Hook->Invalidate();
							return FReply::Handled();
						})
						.ContentPadding(0.0f)
						[
							SNew( SImage )
							.Image(FAppStyle::GetBrush("Icons.Refresh"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				];
		}

		return
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot()
			.Padding(4.f, 2.f)
			.FillEmptySpace(false)
			[
				SNew(SButton)
				.Text_Lambda([this]
				{
					switch (Hook->GetState())
					{
					case EVoxelShaderHookState::Outdated:
					{
						return INVTEXT("Update");
					}
					case EVoxelShaderHookState::NeverApply:
					case EVoxelShaderHookState::Active:
					case EVoxelShaderHookState::NotApplied:
					case EVoxelShaderHookState::Invalid:
					case EVoxelShaderHookState::Deprecated:
					{
						return INVTEXT("Apply");
					}
					default:
					{
						ensure(false);
						return INVTEXT("Apply");
					}
					}
				})
				.IsEnabled_Lambda([this]
				{
					switch (Hook->GetState())
					{
					case EVoxelShaderHookState::NotApplied:
					case EVoxelShaderHookState::Outdated:
					case EVoxelShaderHookState::Invalid:
					{
						return true;
					}
					case EVoxelShaderHookState::NeverApply:
					case EVoxelShaderHookState::Active:
					case EVoxelShaderHookState::Deprecated:
					{
						return false;
					}
					default:
					{
						ensure(false);
						return false;
					}
					}
				})
				.OnClicked_Lambda([this]
				{
					bool bIsCancelled = false;
					if (Hook->Apply(&bIsCancelled))
					{
						FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor").OnApplicationRestartRequired();
					}
					else
					{
						VOXEL_MESSAGE(Error, "Failed to apply hook");
					}

					Hook->Invalidate();

					if (bIsCancelled)
					{
						return FReply::Handled();
					}

					if (Hook->State == EVoxelShaderHookState::Invalid)
					{
						SVoxelShaderHookPatchPopup::ShowDialog(Hook);
					}

					return FReply::Handled();
				})
			]
			+ SWrapBox::Slot()
			.Padding(4.f, 2.f)
			.FillEmptySpace(false)
			[
				SNew(SButton)
				.Text(INVTEXT("Remove"))
				.IsEnabled_Lambda([this]
				{
					switch (Hook->GetState())
					{
					case EVoxelShaderHookState::Active:
					case EVoxelShaderHookState::Outdated:
					{
						return true;
					}
					case EVoxelShaderHookState::NeverApply:
					case EVoxelShaderHookState::NotApplied:
					case EVoxelShaderHookState::Invalid:
					case EVoxelShaderHookState::Deprecated:
					{
						return false;
					}
					default:
					{
						ensure(false);
						return false;
					}
					}
				})
				.OnClicked_Lambda([this]
				{
					if (Hook->Revert())
					{
						FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor").OnApplicationRestartRequired();
					}
					else
					{
						VOXEL_MESSAGE(Error, "Failed to revert");
					}

					Hook->Invalidate();
					return FReply::Handled();
				})
			];
	}

private:
	FVoxelShaderHookGroup* Hook = nullptr;
};

class FVoxelShaderHooksSettingsCustomization : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		for (FVoxelShaderHookGroup* Hook : GVoxelShaderHooksManager->Hooks)
		{
			SharedHooks.Add(MakeSharedCopy(Hook));
		}

		DetailLayout.EditCategory("Shader Hooks").AddCustomRow(INVTEXT("Shader Hooks"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(5.f)
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(INVTEXT("Engine shader changes required by Voxel Plugin features"))
			]
			+ SVerticalBox::Slot()
			.Padding(5.f)
			.FillHeight(1.f)
			[
				SNew(SBox)
				.MaxDesiredHeight(600.f)
				[
					SAssignNew(HooksListView, SListView<TSharedPtr<FVoxelShaderHookGroup*>>)
					.ListItemsSource(&SharedHooks)
					.OnGenerateRow_Lambda([](const TSharedPtr<FVoxelShaderHookGroup*> HookPtr, const TSharedRef<STableViewBase>& OwnerTable)
					{
						return
							SNew(SVoxelShaderHookRow, OwnerTable)
							.Hook(HookPtr ? *HookPtr : nullptr);
					})
					.HeaderRow(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("NeverApply")
						.HAlignCell(HAlign_Center)
						.FixedWidth(30.f)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(FText::GetEmpty())
								.Font(IDetailLayoutBuilder::GetDetailFont())
							]
						]
						+ SHeaderRow::Column("Name")
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.HeaderContentPadding(FMargin(4.f))
						.FillWidth(1.f)
						[
							SNew(SVoxelDetailText)
							.Text(INVTEXT("Name"))
						]
						+ SHeaderRow::Column("Description")
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.HeaderContentPadding(FMargin(4.f))
						.FillWidth(2.f)
						[
							SNew(SVoxelDetailText)
							.Text(INVTEXT("Description"))
						]
						+ SHeaderRow::Column("State")
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.HeaderContentPadding(FMargin(4.f))
						.FillWidth(0.5f)
						[
							SNew(SVoxelDetailText)
							.Text(INVTEXT("State"))
						]
						+ SHeaderRow::Column("Action")
						.HAlignCell(HAlign_Fill)
						.VAlignCell(VAlign_Center)
						.HeaderContentPadding(FMargin(4.f))
						.FillWidth(0.5f)
						[
							SNew(SVoxelDetailText)
							.Text(INVTEXT(" "))
						])
				]
			]
		];
	}

private:
	TSharedPtr<SListView<TSharedPtr<FVoxelShaderHookGroup*>>> HooksListView;
	TArray<TSharedPtr<FVoxelShaderHookGroup*>> SharedHooks;
};

DEFINE_VOXEL_CLASS_LAYOUT(UVoxelShaderHooksSettings, FVoxelShaderHooksSettingsCustomization);