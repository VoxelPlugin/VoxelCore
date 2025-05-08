// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

template<typename OptionType>
class SVoxelSegmentedControl : public SCompoundWidget
{
public:
	struct FSlot
		: public TSlotBase<FSlot>
		, public TAlignmentWidgetSlotMixin<FSlot>
	{
		FSlot(const OptionType& InValue)
			: TSlotBase<FSlot>()
			, TAlignmentWidgetSlotMixin<FSlot>(HAlign_Center, VAlign_Fill)
			, _Text()
			, _Tooltip()
			, _Icon(nullptr)
			, _Value(InValue)
			{}

		SLATE_SLOT_BEGIN_ARGS_OneMixin(FSlot, TSlotBase<FSlot>, TAlignmentWidgetSlotMixin<FSlot>)
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_ATTRIBUTE(FText, ToolTip)
			SLATE_ATTRIBUTE(TSharedPtr<IToolTip>, ToolTipWidget)
			SLATE_ATTRIBUTE(const FSlateBrush*, Icon)
			SLATE_ARGUMENT(TOptional<OptionType>, Value)
		SLATE_SLOT_END_ARGS()

		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs)
		{
			TSlotBase<FSlot>::Construct(SlotOwner, MoveTemp(InArgs));
			TAlignmentWidgetSlotMixin<FSlot>::ConstructMixin(SlotOwner, MoveTemp(InArgs));
			if (InArgs._Text.IsSet())
			{
				_Text = MoveTemp(InArgs._Text);
			}
			if (InArgs._ToolTip.IsSet())
			{
				_Tooltip = MoveTemp(InArgs._ToolTip);
			}
			if (InArgs._ToolTipWidget.IsSet())
			{
				_ToolTipWidget = MoveTemp(InArgs._ToolTipWidget);
			}
			if (InArgs._Icon.IsSet())
			{
				_Icon = MoveTemp(InArgs._Icon);
			}
			if (InArgs._Value.IsSet())
			{
				_Value = MoveTemp(InArgs._Value.GetValue());
			}
		}

		void SetText(TAttribute<FText> InText)
		{
			_Text = MoveTemp(InText);
		}

		FText GetText() const
		{
			return _Text.Get();
		}

		void SetIcon(TAttribute<const FSlateBrush*> InBrush)
		{
			_Icon = MoveTemp(InBrush);
		}

		const FSlateBrush* GetIcon() const
		{
			return _Icon.Get();
		}

		void SetToolTip(TAttribute<FText> InTooltip)
		{
			_Tooltip = MoveTemp(InTooltip);
		}

		FText GetToolTip() const
		{
			return _Tooltip.Get();
		}

	friend SVoxelSegmentedControl<OptionType>;

	private:
		TAttribute<FText> _Text;
		TAttribute<FText> _Tooltip;
		TAttribute<TSharedPtr<IToolTip>> _ToolTipWidget;
		TAttribute<const FSlateBrush*> _Icon;

		OptionType _Value;
		TWeakPtr<SCheckBox> _CheckBox;
	};

	static typename FSlot::FSlotArguments Slot(const OptionType& InValue)
	{
		return typename FSlot::FSlotArguments(MakeUnique<FSlot>(InValue));
	}

	DECLARE_DELEGATE_OneParam(FOnValueChanged, OptionType);
	DECLARE_DELEGATE_TwoParams(FOnValuesChanged, TArray<OptionType>, TArray<OptionType>);

	using FValuesMap = TMap<OptionType, ECheckBoxState>;

	SLATE_BEGIN_ARGS(SVoxelSegmentedControl<OptionType>)
		: _Style(nullptr)
		, _TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallButtonText"))
		, _SupportsMultiSelection(false)
		, _SupportsEmptySelection(false)
		, _MaxSegmentsPerLine(0)
	{}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
		SLATE_STYLE_ARGUMENT(FSegmentedControlStyle, Style)
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ARGUMENT(bool, SupportsMultiSelection)
		SLATE_ARGUMENT(bool, SupportsEmptySelection)
		SLATE_ATTRIBUTE(OptionType, Value)
		SLATE_ATTRIBUTE(FValuesMap, Values)
		SLATE_ATTRIBUTE(FMargin, UniformPadding)
		SLATE_ATTRIBUTE(FMargin, SlotPadding)
		SLATE_ATTRIBUTE(FMargin, BorderPadding)
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
		SLATE_EVENT(FOnValuesChanged, OnValuesChanged)
		SLATE_ARGUMENT(int32, MaxSegmentsPerLine)
		SLATE_ARGUMENT(float, MinDesiredSlotWidth)
	SLATE_END_ARGS()

	SVoxelSegmentedControl()
		: Children(this)
		, CurrentValues(*this)
	{}

	void Construct( const FArguments& InArgs )
	{
		SupportsMultiSelection = InArgs._SupportsMultiSelection;
		if (!InArgs._Style)
		{
			if (SupportsMultiSelection)
			{
				Style = &FVoxelEditorStyle::Get().GetWidgetStyle<FSegmentedControlStyle>("VoxelMultiSegmentedControl");
			}
			else
			{
				Style = &FVoxelEditorStyle::Get().GetWidgetStyle<FSegmentedControlStyle>("VoxelSingleSegmentedControl");
			}
		}
		else
		{
			Style = InArgs._Style;
		}
		TextStyle = InArgs._TextStyle;
		MinDesiredSlotWidth = InArgs._MinDesiredSlotWidth;

		SupportsEmptySelection = InArgs._SupportsEmptySelection;
		CurrentValuesIsBound = false;

		if (InArgs._Value.IsBound() ||
			InArgs._Value.IsSet())
		{
			SetValue(InArgs._Value, false);
		}
		else if (
			InArgs._Values.IsBound() ||
			InArgs._Values.IsSet())
		{
			SetValues(InArgs._Values, false);
		}

		OnValueChanged = InArgs._OnValueChanged;
		OnValuesChanged = InArgs._OnValuesChanged;

		UniformPadding = InArgs._UniformPadding;
		SlotPadding = InArgs._SlotPadding;
		BorderPadding = InArgs._BorderPadding;

		MaxSegmentsPerLine = InArgs._MaxSegmentsPerLine;
		Children.AddSlots(MoveTemp(const_cast<TArray<typename FSlot::FSlotArguments>&>(InArgs._Slots)));
		RebuildChildren();
	}

	void RebuildChildren()
	{
		FMargin InnerSlotPadding = SlotPadding.IsSet() ? SlotPadding.Get() : Style->UniformPadding;
		const float RightPadding = InnerSlotPadding.Right;
		InnerSlotPadding.Right = 0.0f;

		const TSharedRef<SUniformGridPanel> UniformBox =
			SNew(SUniformGridPanel)
			.SlotPadding(InnerSlotPadding)
			.MinDesiredSlotWidth(MinDesiredSlotWidth);

		const int32 NumSlots = Children.Num();
		for (int32 Index = 0; Index < NumSlots; Index++)
		{
			TSharedRef<SWidget> Child = Children[Index].GetWidget();
			FSlot* ChildSlotPtr = &Children[Index];
			const OptionType ChildValue = ChildSlotPtr->_Value;

			TAttribute<FVector2D> SpacerLambda = FVector::ZeroVector;
			if (ChildSlotPtr->_Icon.IsBound() ||
				ChildSlotPtr->_Text.IsBound())
			{
				SpacerLambda = MakeAttributeLambda([ChildSlotPtr]
				{
					return
						ChildSlotPtr->_Icon.Get() &&
						!ChildSlotPtr->_Text.Get().IsEmpty()
						? FVector2D(8.0f, 1.0f)
						: FVector2D::ZeroVector;
				});
			}
			else
			{
				SpacerLambda =
					ChildSlotPtr->_Icon.Get() &&
					!ChildSlotPtr->_Text.Get().IsEmpty()
					? FVector2D(8.0f, 1.0f)
					: FVector2D::ZeroVector;
			}

			if (Child == SNullWidget::NullWidget)
			{
				Child = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(ChildSlotPtr->_Icon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(SpacerLambda)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(TextStyle)
					.Text(ChildSlotPtr->_Text) 
				];
			}

			const FCheckBoxStyle* CheckBoxStyle = &Style->ControlStyle;
			if (Index == 0)
			{
				CheckBoxStyle = &Style->FirstControlStyle;
			}
			else if (Index == NumSlots - 1)
			{
				CheckBoxStyle = &Style->LastControlStyle;
			}

			const int32 ColumnIndex = MaxSegmentsPerLine > 0 ? Index % MaxSegmentsPerLine : Index;
			const int32 RowIndex = MaxSegmentsPerLine > 0 ? Index / MaxSegmentsPerLine : 0;

			// Note HAlignment is applied at the check box level because if it were applied here it would make the slots look physically disconnected from each other 
			UniformBox->AddSlot(ColumnIndex, RowIndex)
			.VAlign(ChildSlotPtr->GetVerticalAlignment())
			[
				SAssignNew(ChildSlotPtr->_CheckBox, SCheckBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				.HAlign(ChildSlotPtr->GetHorizontalAlignment())
				.ToolTipText(ChildSlotPtr->_Tooltip)
				.ToolTip(ChildSlotPtr->_ToolTipWidget)
				.Style(CheckBoxStyle)
				.IsChecked(GetCheckBoxStateAttribute(ChildValue))
				.OnCheckStateChanged(this, &SVoxelSegmentedControl<OptionType>::CommitValue, ChildValue)
				.Padding(UniformPadding)
				[
					Child
				]
			];
		}

		FMargin BackgroundBorderPadding;
		if (BorderPadding.IsSet())
		{
			BackgroundBorderPadding = BorderPadding.Get();
			BackgroundBorderPadding.Left -= InnerSlotPadding.Left;
			BackgroundBorderPadding.Top -= InnerSlotPadding.Top;
			BackgroundBorderPadding.Right -= RightPadding;
			BackgroundBorderPadding.Bottom -= InnerSlotPadding.Bottom;
		}
		else
		{
			BackgroundBorderPadding = FMargin(0.f, 0.f, RightPadding, 0.f);
		}

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(&Style->BackgroundBrush)
			.Padding(BackgroundBorderPadding)
			[
				UniformBox
			]
		];

		UpdateCheckboxValuesIfNeeded();
	}

	// Slot Management
	using FScopedWidgetSlotArguments = typename TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot(const OptionType& InValue, const bool bRebuildChildren = true)
	{
		if (bRebuildChildren)
		{
			TWeakPtr<SVoxelSegmentedControl<OptionType>> AsWeak = SharedThis(this);
			return
				FScopedWidgetSlotArguments
				{
					MakeUnique<FSlot>(InValue),
					this->Children,
					INDEX_NONE,
					[AsWeak](const FSlot*, int32)
					{
						if (TSharedPtr<SVoxelSegmentedControl<OptionType>> SharedThis = AsWeak.Pin())
						{
							SharedThis->RebuildChildren();
						}
					}
				};
		}
		else
		{
			return
				FScopedWidgetSlotArguments
				{
					MakeUnique<FSlot>(InValue),
					this->Children,
					INDEX_NONE
				};
		}
	}

	int32 NumSlots() const
	{
		return Children.Num();
	}

	OptionType GetValue() const
	{
		const TMap<OptionType, ECheckBoxState> Values = GetValues();
		if (Values.IsEmpty())
		{
			return OptionType();
		}

		for (const auto& It : Values)
		{
			if (It.Value == ECheckBoxState::Checked)
			{
				return It.Key;
			}
		}

		for (const auto& It : Values)
		{
			if (It.Value == ECheckBoxState::Undetermined)
			{
				return It.Key;
			}
		}

		return OptionType();
	}

	TMap<OptionType, ECheckBoxState> GetValues() const
	{
		return CurrentValues.Get();
	}

	bool HasValue(OptionType InValue)
	{
		return GetValues().Contains(InValue);
	}

	void SetValue(TAttribute<OptionType> InValue, bool bUpdateChildren = true)
	{
		if (InValue.IsBound())
		{
			SetValues(TAttribute<FValuesMap>::CreateLambda([InValue]() -> FValuesMap
			{
				FValuesMap Values;
				Values.Add(InValue.Get(), ECheckBoxState::Checked);
				return Values;
			}), bUpdateChildren);
			return;
		}

		if (InValue.IsSet())
		{
			FValuesMap Values;
			Values.Add(InValue.Get(), ECheckBoxState::Checked);
			SetValues(TAttribute<FValuesMap>(Values), bUpdateChildren);
			return;
		}

		SetValues(TAttribute<FValuesMap>(), bUpdateChildren);
	}

	void SetValues(TAttribute<FValuesMap> InValues, const bool bUpdateChildren = true) 
	{
		CurrentValuesIsBound = InValues.IsBound();

		if (CurrentValuesIsBound)
		{
			CurrentValues.Assign(*this, InValues);
		}
		else if (InValues.IsSet())
		{
			CurrentValues.Set(*this, InValues.Get());
		}
		else
		{
			CurrentValues.Unbind(*this);
		}

		if (bUpdateChildren)
		{
			UpdateCheckboxValuesIfNeeded();
		}
	}

private:
	TAttribute<ECheckBoxState> GetCheckBoxStateAttribute(OptionType InValue) const
	{
		auto Lambda = [this, InValue]()
		{
			const TMap<OptionType, ECheckBoxState> Values = GetValues();
			if (!Values.Contains(InValue))
			{
				return ECheckBoxState::Unchecked;
			}

			return Values[InValue];
		};

		if (CurrentValuesIsBound)
		{
			return MakeAttributeLambda(Lambda);
		}

		return Lambda();
	}

	void UpdateCheckboxValuesIfNeeded()
	{
		if (CurrentValuesIsBound)
		{
			return;
		}

		const TMap<OptionType, ECheckBoxState> Values = GetValues();
		for (int32 Index = 0; Index < Children.Num(); ++Index)
		{
			const FSlot& Slot = Children[Index];
			if (const TSharedPtr<SCheckBox> CheckBox = Slot._CheckBox.Pin())
			{
				ECheckBoxState State = ECheckBoxState::Unchecked;
				if (const ECheckBoxState* StatePtr = Values.Find(Slot._Value))
				{
					State = *StatePtr;
				}

				CheckBox->SetIsChecked(State);
			}
		}
	}

	void CommitValue(const ECheckBoxState InCheckState, OptionType InValue)
	{
		FValuesMap Values = CurrentValues.Get();

		if (InCheckState != ECheckBoxState::Checked &&
			Values.Num() == 1)
		{
			if (!SupportsEmptySelection)
			{
				UpdateCheckboxValuesIfNeeded();
				return;
			}
		}

		bool bModifierIsDown = false;
		if (SupportsMultiSelection)
		{
			bModifierIsDown =
				FSlateApplication::Get().GetModifierKeys().IsShiftDown() ||
				FSlateApplication::Get().GetModifierKeys().IsControlDown();
		}

		TArray<OptionType> AddedValues;
		TArray<OptionType> RemovedValues;

		const bool bSingleValueChanged =
			InCheckState == ECheckBoxState::Checked ||
			(!SupportsMultiSelection && Values.Num() > 1);

		// If modifier is down -> select exact single value (for multi selection)
		// When multi selection is not supported and there are multiple values, assign single value
		if (bModifierIsDown ||
			(!SupportsMultiSelection && Values.Num() > 1))
		{
			AddedValues.Add(InValue);
			for (const auto& It : Values)
			{
				if (It.Key == InValue)
				{
					continue;
				}

				RemovedValues.Add(It.Key);
			}

			Values.Reset();
			Values.Add(InValue, ECheckBoxState::Checked);
		}
		else
		{
			if (InCheckState == ECheckBoxState::Checked)
			{
				AddedValues.Add(InValue);
				Values.Add(InValue, ECheckBoxState::Checked);
			}
			else
			{
				RemovedValues.Add(InValue);
				Values.Remove(InValue);
			}
		}

		if (!CurrentValuesIsBound)
		{
			CurrentValues.Set(*this, Values);
			UpdateCheckboxValuesIfNeeded();
		}

		if (bSingleValueChanged)
		{
			OnValueChanged.ExecuteIfBound(InValue);
		}

		OnValuesChanged.ExecuteIfBound(AddedValues, RemovedValues);
	}

private:
	TPanelChildren<FSlot> Children;

	FOnValueChanged OnValueChanged;
	FOnValuesChanged OnValuesChanged;

	struct TSlateAttributeMapComparePredicate
	{
		static bool IdenticalTo(const SWidget&, const TMap<OptionType, ECheckBoxState>& Lhs, const TMap<OptionType, ECheckBoxState>& Rhs)
		{
			return Lhs.OrderIndependentCompareEqual(Rhs);
		}
	};
	TSlateAttribute<TMap<OptionType, ECheckBoxState>, EInvalidateWidgetReason::Paint, TSlateAttributeMapComparePredicate> CurrentValues;

	TAttribute<FMargin> UniformPadding;
	TAttribute<FMargin> SlotPadding;
	TAttribute<FMargin> BorderPadding;

	const FSegmentedControlStyle* Style = nullptr;

	const FTextBlockStyle* TextStyle = nullptr;

	int32 MaxSegmentsPerLine = 0;

	bool CurrentValuesIsBound = false;
	bool SupportsMultiSelection = false;
	bool SupportsEmptySelection = false;

	float MinDesiredSlotWidth = 0.f;
};