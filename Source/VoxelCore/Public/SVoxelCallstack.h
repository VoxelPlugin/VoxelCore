// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

#if WITH_EDITOR
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

struct VOXELCORE_API FVoxelCallstackEntry
{
	enum class EType
	{
		Default,
		Subdued,
		Marked
	};

	FString Name;
	FString Prefix;
	EType Type = EType::Default;
	TArray<TSharedPtr<FVoxelCallstackEntry>> Children;

	FVoxelCallstackEntry(
		const FString& Name,
		const FString& Prefix,
		const EType Type)
		: Name(Name)
		, Prefix(Prefix)
		, Type(Type)
	{
	}

	virtual void OnClick() const {}
	virtual ~FVoxelCallstackEntry() = default;
};

struct VOXELCORE_API FVoxelCallstackObjectEntry : public FVoxelCallstackEntry
{
	FVoxelObjectPtr WeakObject;

	FVoxelCallstackObjectEntry(
		const FVoxelObjectPtr& Object,
		const FString& Name,
		const FString& Prefix,
		const EType Type)
		: FVoxelCallstackEntry(Name, Prefix, Type)
		, WeakObject(Object)
	{
	}

	virtual void OnClick() const
	{
		FVoxelUtilities::FocusObject(WeakObject.Resolve());
	}
};

class VOXELCORE_API SVoxelCallstack : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(FString, Title);
		SLATE_ARGUMENT(TSharedPtr<SWindow>, Window)
		SLATE_EVENT(TDelegate<TArray<TSharedPtr<FVoxelCallstackEntry>>()>, OnCollectEntries)
	};

	void Construct(const FArguments& Args);

public:
	static void CreatePopup(
		const FString& Title,
		const TFunction<TArray<TSharedPtr<FVoxelCallstackEntry>>()>& CollectEntries);

private:
	TWeakPtr<SWindow> WeakWindow;
	TArray<TSharedPtr<FVoxelCallstackEntry>> Entries;

	TSharedPtr<STreeView<TSharedPtr<FVoxelCallstackEntry>>> TreeView;
};
#endif