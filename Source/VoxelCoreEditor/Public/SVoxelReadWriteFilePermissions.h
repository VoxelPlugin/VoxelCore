// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"

class VOXELCOREEDITOR_API SVoxelReadWriteFilePermissionsNotice : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		FArguments()
			: _InvalidationRate(1.f)
			, _InProgressInvalidationRate(0.5f)
			, _WrapTextAt(350.f)
		{}

		SLATE_ARGUMENT(float, InvalidationRate)
		SLATE_ARGUMENT(float, InProgressInvalidationRate)
		SLATE_ARGUMENT(float, WrapTextAt)
		SLATE_ATTRIBUTE(FString, FilePath)
	};

	void Construct(const FArguments& InArgs);
	void Invalidate();
	bool IsUnlocked() const;
	void SetFilePath(const TAttribute<FString>& FilePath);

protected:
	//~ Begin SCompoundWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
	//~ End SCompoundWidget Interface

	private:
	float InvalidationRate = 1.f;
	float InProgressInvalidationRate = 0.5f;
	TAttribute<FString> FilePathAttribute;

	float LastCheckTime = 0.f;
	bool bFixupRequired = false;
	bool bFixupInProgress = false;
	bool bFixupPossible = false;

public:
	static void GetFileStatus(const FString& FilePath, bool& bOutFixupRequired, bool& bOutFixupInProgress, bool& bOutFixupPossible);
};

class VOXELCOREEDITOR_API SVoxelReadWriteFilePermissionsPopup : public SCompoundWidget
{
public:
	VOXEL_SLATE_ARGS()
	{
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(FString, FilePath)
	};

	void Construct(const FArguments& InArgs);

	static bool PromptForPermissions(const FString& FilePath);

private:
	TWeakPtr<SWindow> WeakParentWindow;
	TSharedPtr<SVoxelReadWriteFilePermissionsNotice> PermissionsNotice;

public:
	bool bContinue = false;
};