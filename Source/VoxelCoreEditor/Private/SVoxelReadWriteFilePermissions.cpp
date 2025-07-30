// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelReadWriteFilePermissions.h"
#include "ISourceControlModule.h"
#include "SSettingsEditorCheckoutNotice.h"

void SVoxelReadWriteFilePermissionsNotice::Construct(const FArguments& InArgs)
{
	InvalidationRate = InArgs._InvalidationRate;
	InProgressInvalidationRate = InArgs._InProgressInvalidationRate;
	FilePathAttribute = InArgs._FilePath;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.f)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image_Lambda(MakeWeakPtrLambda(this, [this]
			{
				return FAppStyle::Get().GetBrush(bFixupRequired ? "Icons.Lock" : "Icons.Unlock");
			}))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 8.f, 8.f, 8.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.WrapTextAt(InArgs._WrapTextAt)
			.Text_Lambda(MakeWeakPtrLambda(this, [this]
			{
				FString FileName = FPaths::GetCleanFilename(FilePathAttribute.Get());
				// We don't want extension for Assets
				FileName.RemoveFromEnd(".uasset");

				if (bFixupInProgress)
				{
					return INVTEXT("Checking file state...");
				}

				if (bFixupRequired)
				{
					if (ISourceControlModule::Get().IsEnabled())
					{
						if (bFixupPossible)
						{
							return FText::FromString(FileName + " is not checked out.");
						}
						else
						{
							return FText::FromString(FileName + " is checked out by someone else.");
						}
					}
					else
					{
						return FText::FromString(FileName + " is read-only.");
					}
				}

				if (ISourceControlModule::Get().IsEnabled())
				{
					return FText::FromString(FileName + " is checked out.");
				}
				else
				{
					return FText::FromString(FileName + " is writeable.");
				}
			}))
			.ColorAndOpacity_Lambda(MakeWeakPtrLambda(this, [this]
			{
				if (bFixupRequired &&
					!bFixupPossible)
				{
					return FAppStyle::GetSlateColor(STATIC_FNAME("Colors.AccentYellow"));
				}
				return FAppStyle::GetSlateColor(!bFixupRequired || bFixupInProgress ? STATIC_FNAME("Colors.Foreground") : STATIC_FNAME("Colors.AccentYellow"));
			}))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Visibility_Lambda(MakeWeakPtrLambda(this, [this]
			{
				return bFixupRequired && !bFixupInProgress && bFixupPossible ? EVisibility::Visible : EVisibility::Collapsed;
			}))
			.Text_Lambda(MakeWeakPtrLambda(this, []
			{
				if (ISourceControlModule::Get().IsEnabled() &&
					ISourceControlModule::Get().GetProvider().IsAvailable())
				{
					return INVTEXT("Check Out File");
				}

				return INVTEXT("Make Writable");
			}))
			.OnClicked_Lambda(MakeWeakPtrLambda(this, [this]() -> FReply
			{
				const FString FilePath = FilePathAttribute.Get();

				bool bSuccess = SettingsHelpers::CheckOutOrAddFile(FilePath);
				if (!bSuccess)
				{
					bSuccess = SettingsHelpers::MakeWritable(FilePath);
				}

				if (bSuccess)
				{
					bFixupRequired = false;
				}

				return FReply::Handled();
			}, FReply::Handled()))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SThrobber)
			.Visibility_Lambda(MakeWeakPtrLambda(this, [this]
			{
				return bFixupInProgress ? EVisibility::Visible : EVisibility::Collapsed;
			}))
		]
	];
}

void SVoxelReadWriteFilePermissionsNotice::Invalidate()
{
	LastCheckTime = -1.f;
}

bool SVoxelReadWriteFilePermissionsNotice::IsUnlocked() const
{
	return
		!bFixupRequired &&
		!bFixupInProgress;
}

void SVoxelReadWriteFilePermissionsNotice::SetFilePath(const TAttribute<FString>& FilePath)
{
	FilePathAttribute = FilePath;
}

void SVoxelReadWriteFilePermissionsNotice::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (FSlateApplication::Get().GetActiveModalWindow())
	{
		ISourceControlModule::Get().Tick();
	}

	const float Rate = bFixupInProgress ? InProgressInvalidationRate : InvalidationRate;
	if (InCurrentTime - LastCheckTime < Rate)
	{
		return;
	}

	GetFileStatus(FilePathAttribute.Get(), bFixupRequired, bFixupInProgress, bFixupPossible);
	LastCheckTime = InCurrentTime;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelReadWriteFilePermissionsNotice::GetFileStatus(const FString& FilePath, bool& bOutFixupRequired, bool& bOutFixupInProgress, bool& bOutFixupPossible)
{
	if (FilePath.IsEmpty())
	{
		bOutFixupRequired = false;
		bOutFixupInProgress = false;
		bOutFixupPossible = false;
		return;
	}

	if (ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlModule::Get().QueueStatusUpdate(FilePath);

		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
		if (const FSourceControlStatePtr State = Provider.GetState(FilePath, EStateCacheUsage::Use))
		{
			bOutFixupRequired = !State->IsCheckedOut() && !State->IsLocal();
			bOutFixupInProgress = State->IsUnknown();
			bOutFixupPossible = State->CanCheckout() || State->IsLocal();
		}
		else
		{
			bOutFixupRequired = false;
			bOutFixupInProgress = false;
			bOutFixupPossible = false;
		}
		return;
	}

	bOutFixupRequired =
		FPaths::FileExists(FilePath) &&
		IFileManager::Get().IsReadOnly(*FilePath);
	bOutFixupInProgress = false;
	bOutFixupPossible = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelReadWriteFilePermissionsPopup::Construct(const FArguments& InArgs)
{
	WeakParentWindow = InArgs._ParentWindow;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(16.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(16.f, 0.f, 0.f, 0.f)
				.MaxWidth(550.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SAssignNew(PermissionsNotice, SVoxelReadWriteFilePermissionsNotice)
						.FilePath(InArgs._FilePath)
						.WrapTextAt(550.f)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(INVTEXT("Continue"))
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.IsEnabled_Lambda(MakeWeakPtrLambda(this, [this]
					{
						if (PermissionsNotice)
						{
							return PermissionsNotice->IsUnlocked();
						}

						return false;
					}))
					.OnClicked_Lambda(MakeWeakPtrLambda(this, [this]
					{
						bContinue = true;
						if (const TSharedPtr<SWindow> PinnedWindow = WeakParentWindow.Pin())
						{
							PinnedWindow->RequestDestroyWindow();
						}
						return FReply::Handled();
					}, FReply::Handled()))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(INVTEXT("Cancel"))
					.OnClicked_Lambda(MakeWeakPtrLambda(this, [this]
					{
						if (const TSharedPtr<SWindow> PinnedWindow = WeakParentWindow.Pin())
						{
							PinnedWindow->RequestDestroyWindow();
						}
						return FReply::Handled();
					}, FReply::Handled()))
				]
			]
		]
	];
}

bool SVoxelReadWriteFilePermissionsPopup::PromptForPermissions(const FString& FilePath)
{
	bool bFixupRequired = false;
	bool bFixupInProgress = false;
	bool bFixupPossible = false;
	SVoxelReadWriteFilePermissionsNotice::GetFileStatus(FilePath, bFixupRequired, bFixupInProgress, bFixupPossible);

	if (!bFixupRequired &&
		!bFixupInProgress)
	{
		return true;
	}

	const TSharedRef<SWindow> EditWindow = SNew(SWindow)
		.Title(INVTEXT("File Permissions"))
		.SizingRule(ESizingRule::Autosized)
		.ClientSize(FVector2D(0.f, 300.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	const TSharedRef<SVoxelReadWriteFilePermissionsPopup> FilePermissionsPopup = SNew(SVoxelReadWriteFilePermissionsPopup)
		.ParentWindow(EditWindow)
		.FilePath(FilePath);

	EditWindow->SetContent(FilePermissionsPopup);

	GEditor->EditorAddModalWindow(EditWindow);

	return FilePermissionsPopup->bContinue;
}