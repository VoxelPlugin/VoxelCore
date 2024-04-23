// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"
#include "SVoxelDetailWidgets.h"
#include "VoxelEditorUtilities.h"

template<typename T>
class SVoxelDetailComboBox
	: public SCompoundWidget
	, public FSelfRegisteringEditorUndoClient
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelection, T);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsOptionValid, T);
	DECLARE_DELEGATE_RetVal_OneParam(FString, FGetOptionText, T);
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetOptionToolTip, T);
	DECLARE_DELEGATE_RetVal_OneParam(T, FOnMakeOptionFromText, FString);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerate, T);

	VOXEL_SLATE_ARGS()
	{
		FArguments()
		{
			_CurrentOption = T{};
			_CanEnterCustomOption = false;

			if constexpr (std::is_same_v<T, FName> || std::is_same_v<T, FString>)
			{
				_OptionText = MakeLambdaDelegate([](T Value)
				{
					return LexToString(Value);
				});
				_OnMakeOptionFromText = MakeLambdaDelegate([](FString String)
				{
					T Value;
					LexFromString(Value, *String);
					return Value;
				});
			};
		}

		SLATE_ARGUMENT(FSimpleDelegate, RefreshDelegate);

		bool _NoRefreshDelegate = false;
		WidgetArgsType& NoRefreshDelegate()
		{
			_NoRefreshDelegate = true;
			return *this;
		}

		template<typename ArgType>
		WidgetArgsType& RefreshDelegate(IDetailCustomization* DetailCustomization, const ArgType& Arg)
		{
			_RefreshDelegate = FVoxelEditorUtilities::MakeRefreshDelegate(DetailCustomization, Arg);
			return *this;
		}
		template<typename ArgType>
		WidgetArgsType& RefreshDelegate(IPropertyTypeCustomization* DetailCustomization, const ArgType& Arg)
		{
			_RefreshDelegate = FVoxelEditorUtilities::MakeRefreshDelegate(DetailCustomization, Arg);
			return *this;
		}

		SLATE_ATTRIBUTE(TArray<T>, Options);
		SLATE_ATTRIBUTE(T, CurrentOption);

		SLATE_ARGUMENT(bool, CanEnterCustomOption);
		SLATE_EVENT(FOnMakeOptionFromText, OnMakeOptionFromText);

		SLATE_EVENT(FGetOptionText, OptionText)
		SLATE_EVENT(FGetOptionToolTip, OptionToolTip);
		SLATE_EVENT(FIsOptionValid, IsOptionValid);

		SLATE_EVENT(FOnSelection, OnSelection);
		SLATE_EVENT(FOnGenerate, OnGenerate);
	};

	void Construct(const FArguments& Args)
	{
		OptionsAttribute = Args._Options;
		CurrentOptionAttribute = Args._CurrentOption;

		for (T Option : OptionsAttribute.Get())
		{
			Options.Add(MakeVoxelShared<T>(Option));
		}

		bCanEnterCustomOption = Args._CanEnterCustomOption;
		if (bCanEnterCustomOption)
		{
			ensure(Args._OnMakeOptionFromText.IsBound());
		}

		OnMakeOptionFromTextDelegate = Args._OnMakeOptionFromText;
		GetOptionTextDelegate = Args._OptionText;
		GetOptionToolTipDelegate = Args._OptionToolTip;
		IsOptionValidDelegate = Args._IsOptionValid;
		OnSelectionDelegate = Args._OnSelection;
		OnGenerateDelegate = Args._OnGenerate;

		ensure(Args._NoRefreshDelegate || Args._RefreshDelegate.IsBound());
		RefreshDelegate = Args._RefreshDelegate;

		TSharedPtr<SWidget> ComboboxContent;
		if (bCanEnterCustomOption)
		{
			SAssignNew(CustomOptionTextBox, SEditableTextBox)
			.Text_Lambda([=, this]() -> FText
			{
				if (!GetOptionTextDelegate.IsBound())
				{
					return {};
				}

				return FText::FromString(GetOptionTextDelegate.Execute(CurrentOptionAttribute.Get()));
			})
			.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
			{
				if (!ensure(OnMakeOptionFromTextDelegate.IsBound()))
				{
					return;
				}

				const T NewOption = OnMakeOptionFromTextDelegate.Execute(Text.ToString());

				TSharedPtr<T> NewOptionPtr;
				for (const TSharedPtr<T>& Option : Options)
				{
					if (NewOption == *Option)
					{
						NewOptionPtr = Option;
						break;
					}
				}

				if (!NewOptionPtr)
				{
					NewOptionPtr = MakeVoxelShared<T>(NewOption);
				}

				ComboBox->SetSelectedItem(NewOptionPtr);
			})
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.Font(FVoxelEditorUtilities::Font());

			ComboboxContent = CustomOptionTextBox;
		}
		else
		{
			SAssignNew(SelectedItemBox, SBox)
			[
				GenerateWidget(GetCurrentOption())
			];

			ComboboxContent = SelectedItemBox;
		}

		ChildSlot
		[
			SAssignNew(ComboBox, SComboBox<TSharedPtr<T>>)
			.OptionsSource(&Options)
			.OnSelectionChanged_Lambda([this](TSharedPtr<T> NewOption, ESelectInfo::Type SelectInfo)
			{
				if (!NewOption)
				{
					// Will happen when doing SetSelectedItem(nullptr) below
					return;
				}

				const TWeakPtr<SWidget> WeakThis = AsWeak();
				check(WeakThis.IsValid());

				OnSelectionDelegate.ExecuteIfBound(*NewOption);

				if (!WeakThis.IsValid())
				{
					// OnSelectionDelegate destroyed our widget
					return;
				}

				RefreshOptionsList();

				if (!bCanEnterCustomOption)
				{
					SelectedItemBox->SetContent(GenerateWidget(GetCurrentOption()));
				}
			})
			.OnGenerateWidget_Lambda([this](const TSharedPtr<T>& Option) -> TSharedRef<SWidget>
			{
				return GenerateWidget(Option);
			})
			.InitiallySelectedItem(GetCurrentOption())
			[
				ComboboxContent.ToSharedRef()
			]
		];
	}

public:
	void SetCurrentItem(T NewSelection)
	{
		if (CurrentOptionAttribute.Get() == NewSelection)
		{
			return;
		}

		TSharedPtr<T> NewOption = nullptr;
		for (const TSharedPtr<T>& Option : Options)
		{
			if (*Option == NewSelection)
			{
				NewOption = Option;
			}
		}

		ComboBox->SetSelectedItem(NewOption);
	}

private:
	TSharedPtr<SComboBox<TSharedPtr<T>>> ComboBox;
	TSharedPtr<SBox> SelectedItemBox;
	TSharedPtr<SEditableTextBox> CustomOptionTextBox;

	TAttribute<TArray<T>> OptionsAttribute;
	TAttribute<T> CurrentOptionAttribute;

	TArray<TSharedPtr<T>> Options;
	bool bCanEnterCustomOption = false;

	FOnMakeOptionFromText OnMakeOptionFromTextDelegate;
	FGetOptionText GetOptionTextDelegate;
	FGetOptionToolTip GetOptionToolTipDelegate;
	FIsOptionValid IsOptionValidDelegate;
	FOnSelection OnSelectionDelegate;
	FOnGenerate OnGenerateDelegate;

	TSharedRef<SWidget> GenerateWidget(const TSharedPtr<T>& Option) const
	{
		if (OnGenerateDelegate.IsBound())
		{
			const TSharedRef<SWidget> Widget = OnGenerateDelegate.Execute(*Option);
			Widget->SetClipping(EWidgetClipping::ClipToBounds);
			return Widget;
		}

		return SNew(SVoxelDetailText)
			.Clipping(EWidgetClipping::ClipToBounds)
			.Text_Lambda([=, this]() -> FText
			{
				if (!GetOptionTextDelegate.IsBound())
				{
					return {};
				}

				return FText::FromString(GetOptionTextDelegate.Execute(*Option));
			})
			.ToolTipText_Lambda([=, this]() -> FText
			{
				if (!GetOptionToolTipDelegate.IsBound())
				{
					return {};
				}

				return GetOptionToolTipDelegate.Execute(*Option);
			})
			.ColorAndOpacity_Lambda([=, this]() -> FSlateColor
			{
				if (!Options.Contains(Option))
				{
					return FLinearColor::Red;
				}
				if (IsOptionValidDelegate.IsBound() &&
					!IsOptionValidDelegate.Execute(*Option))
				{
					return FLinearColor::Red;
				}

				return FSlateColor::UseForeground();
			});
	}

public:
	void RefreshOptionsList()
	{
		VOXEL_FUNCTION_COUNTER();

		TArray<T> NewOptions = OptionsAttribute.Get();

		// Remove non-existing options
		Options.RemoveAll([&](const TSharedPtr<T>& Option)
		{
			if (NewOptions.Contains(*Option))
			{
				NewOptions.Remove(*Option);
				return false;
			}
			return true;
		});

		// Add new options
		for (const T& Option : NewOptions)
		{
			Options.Add(MakeVoxelShared<T>(Option));
		}
		ComboBox->RefreshOptions();
	}

private:
	FSimpleDelegate RefreshDelegate;

	TSharedPtr<T> GetCurrentOption() const
	{
		const T CurrentOption = CurrentOptionAttribute.Get();
		for (const TSharedPtr<T>& Option : Options)
		{
			if (*Option == CurrentOption)
			{
				return Option;
			}
		}
		return MakeVoxelShared<T>(CurrentOption);
	}

	//~ Begin Interface FSelfRegisteringEditorUndoClient
	virtual void PostUndo(bool bSuccess) override
	{
		(void)RefreshDelegate.ExecuteIfBound();
	}
	virtual void PostRedo(bool bSuccess) override
	{
		(void)RefreshDelegate.ExecuteIfBound();
	}
	//~ End Interface FSelfRegisteringEditorUndoClient
};