// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelCallstack.h"

#if WITH_EDITOR
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"

class SVoxelExpanderArrow : public SExpanderArrow
{
public:
	void Construct(const FArguments& InArgs, const TSharedPtr<ITableRow>& TableRow)
	{
		SExpanderArrow::Construct(InArgs, TableRow);
		ExpanderArrow->SetVisibility(EVisibility::Hidden);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		static constexpr float WireThickness = 2.0f;
		static constexpr float HalfWireThickness = WireThickness / 2.0f;

		static const FName NAME_VerticalBarBrush = TEXT("WhiteBrush");
		const float Indent = UE_507_SWITCH(IndentAmount, GetIndentAmountAttribute()).Get(10.f);
		const FSlateBrush* VerticalBarBrush = !StyleSet ? nullptr : StyleSet->GetBrush(NAME_VerticalBarBrush);

		if (!ShouldDrawWires.Get() ||
			!VerticalBarBrush)
		{
			LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
			return LayerId;
		}

		const TSharedPtr<ITableRow> OwnerRow = OwnerRowPtr.Pin();
		FLinearColor WireTint = InWidgetStyle.GetForegroundColor();
		WireTint.A = 0.15f;

		// Draw vertical wires to indicate paths to parent nodes.
		const TBitArray<>& NeedsWireByLevel = OwnerRow->GetWiresNeededByDepth();
		const int32 NumLevels = NeedsWireByLevel.Num();
		for (int32 Level = 0; Level < NumLevels; ++Level)
		{
			const float CurrentIndent = Indent * Level;

			if (NeedsWireByLevel[Level])
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(
						FVector2D(WireThickness, AllottedGeometry.Size.Y),
						FSlateLayoutTransform(FVector2D(CurrentIndent - 3.f, 0))),
					VerticalBarBrush,
					ESlateDrawEffect::None,
					WireTint
				);
			}
		}

		const float HalfCellHeight = 0.5f * AllottedGeometry.Size.Y;

		if (OwnerRow->IsLastChild())
		{
			const float CurrentIndent = Indent * (NumLevels - 1);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(WireThickness, HalfCellHeight + HalfWireThickness),
					FSlateLayoutTransform(FVector2D(CurrentIndent - 3.f, 0))),
				VerticalBarBrush,
				ESlateDrawEffect::None,
				WireTint
			);
		}

		if (OwnerRow->IsItemExpanded() && OwnerRow->DoesItemHaveChildren())
		{
			const float CurrentIndent = Indent * NumLevels;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(WireThickness, HalfCellHeight - HalfWireThickness),
					FSlateLayoutTransform(FVector2D(CurrentIndent - 3.f, HalfCellHeight + HalfWireThickness))),
				VerticalBarBrush,
				ESlateDrawEffect::None,
				WireTint
			);
		}

		// Draw horizontal connector from parent wire to child.
		const float LeafDepth = NumLevels < 2 ? 8.f : 0.0f;
		const float HorizontalWireStart = (NumLevels - 1) * Indent;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(AllottedGeometry.Size.X - HorizontalWireStart - WireThickness - LeafDepth, WireThickness),
				FSlateLayoutTransform(FVector2D(HorizontalWireStart + WireThickness - 3.f + LeafDepth, 0.5f * (AllottedGeometry.Size.Y - WireThickness)))
			),
			VerticalBarBrush,
			ESlateDrawEffect::None,
			WireTint
		);

		LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		return LayerId;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SVoxelCallstackRow : public STableRow<TSharedPtr<FVoxelCallstackEntry>>
{
public:
	void Construct(
		const FArguments& Args,
		const TSharedRef<STableViewBase>& OwnerTable,
		const TSharedPtr<FVoxelCallstackEntry>& InItem)
	{
		Entry = InItem;

		STableRow::Construct(
			FArguments()
			.ShowWires(true)
			.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
			.Padding(FMargin(0.f, 2.f)),
			OwnerTable);
	}

	virtual void ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent) override
	{
		// FVoxelEditorStyle::Get()
		const ISlateStyle* VoxelStyle = FSlateStyleRegistry::FindSlateStyle("VoxelStyle");

		const FString StyleName = INLINE_LAMBDA -> FString
		{
			switch (Entry->Type)
			{
			case FVoxelCallstackEntry::EType::Default: return "Callstack.Default";
			case FVoxelCallstackEntry::EType::Subdued: return "Callstack.Subdued";
			case FVoxelCallstackEntry::EType::Marked: return "Callstack.Marked";
			}
			return "";
		};

		SHorizontalBox::FSlot* InnerContentSlotNativePtr = nullptr;

		ExpanderArrowWidget =
			SNew(SVoxelExpanderArrow, SharedThis(this))
			.StyleSet(ExpanderStyleSet)
			.ShouldDrawWires(true);

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				ExpanderArrowWidget.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.Expose(InnerContentSlotNativePtr)
			.Padding(InPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Prefix))
					.TextStyle(VoxelStyle, *StyleName)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(SBox)
					.MinDesiredWidth(150.f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SHyperlink)
						.Text(FText::FromString(Entry->Name))
						.Style(VoxelStyle, *FString(StyleName + ".Hyperlink"))
						.OnNavigate_Lambda([=, this]
						{
							Entry->OnClick();
						})
					]
				]
			]
		];

		InnerContentSlot = InnerContentSlotNativePtr;
	}

private:
	TSharedPtr<FVoxelCallstackEntry> Entry;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelCallstack::Construct(const FArguments& Args)
{
	WeakWindow = Args._Window;

	SetCursor(EMouseCursor::CardinalCross);

	if (ensure(Args._OnCollectEntries.IsBound()))
	{
		Entries = Args._OnCollectEntries.Execute();
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Menu.Background"))
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FAppStyle::GetOptionalBrush("Menu.Outline", nullptr))
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Padding(0.f)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.ForegroundColor(FAppStyle::GetSlateColor("DefaultForeground"))
			[
				SNew(SBox)
				.MaxDesiredHeight(720)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(5.f, 2.f, 2.f, 2.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Args._Title))
							.TextStyle(FAppStyle::Get(), "MessageLog")
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab").CloseButtonStyle)
							.OnClicked_Lambda([this]
							{
								const TSharedPtr<SWindow> Window = WeakWindow.Pin();
								if (!ensure(Window))
								{
									return FReply::Handled();
								}

								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
							.Cursor(EMouseCursor::Default)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					.Padding(2.f)
					[
						SAssignNew(TreeView, STreeView<TSharedPtr<FVoxelCallstackEntry>>)
						.TreeViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("PropertyTable.InViewport.ListView"))
						.TreeItemsSource(&Entries)
						.OnGetChildren_Lambda([](const TSharedPtr<const FVoxelCallstackEntry>& Item, TArray<TSharedPtr<FVoxelCallstackEntry>>& OutChildren)
						{
							OutChildren = Item->Children;
						})
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow_Lambda([this](TSharedPtr<FVoxelCallstackEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
						{
							return SNew(SVoxelCallstackRow, OwnerTable, Item);
						})
					]
				]
			]
		]
	];

	TArray<TSharedPtr<FVoxelCallstackEntry>> EntriesToExpand = Entries;
	while (EntriesToExpand.Num() > 0)
	{
		TArray<TSharedPtr<FVoxelCallstackEntry>> NewEntriesToExpand;
		for (const TSharedPtr<FVoxelCallstackEntry>& Entry : EntriesToExpand)
		{
			TreeView->SetItemExpansion(Entry, true);

			if (Entry->Type == FVoxelCallstackEntry::EType::Marked)
			{
				TreeView->RequestScrollIntoView(Entry);
			}

			NewEntriesToExpand.Append(Entry->Children);
		}

		EntriesToExpand = NewEntriesToExpand;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SVoxelCallstack::CreatePopup(
	const FString& Title,
	const TFunction<TArray<TSharedPtr<FVoxelCallstackEntry>>()>& CollectEntries)
{
	const TSharedRef<SWindow> Window =
		SNew(SWindow)
		.Type(EWindowType::Menu)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsPopupWindow(true)
		.bDragAnywhere(true)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::Autosized)
		.SupportsTransparency(EWindowTransparency::PerPixel);

	const TSharedRef<SVoxelCallstack> CallstackWidget =
		SNew(SVoxelCallstack)
		.Title(Title)
		.Window(Window)
		.OnCollectEntries_Lambda([CollectEntries]
		{
			return CollectEntries();
		});

	Window->SetContent(CallstackWidget);

	const TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (!ensure(RootWindow))
	{
		return;
	}

	FSlateApplication::Get().AddWindowAsNativeChild(Window, RootWindow.ToSharedRef());
	Window->BringToFront();

	FSlateApplication::Get().SetKeyboardFocus(CallstackWidget, EFocusCause::SetDirectly);
}
#endif