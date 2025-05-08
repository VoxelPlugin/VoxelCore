// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class FVoxelDetailInterface;
class FVoxelInstancedStructDataProvider;

class VOXELCOREEDITOR_API FVoxelInstancedStructDetailsWrapper : public TSharedFromThis<FVoxelInstancedStructDetailsWrapper>
{
public:
	static TSharedPtr<FVoxelInstancedStructDetailsWrapper> Make(const TSharedRef<IPropertyHandle>& InstancedStructHandle);

	VOXEL_COUNT_INSTANCES();

	TArray<TSharedPtr<IPropertyHandle>> AddChildStructure();

	IDetailPropertyRow* AddExternalStructure(
		const FVoxelDetailInterface& DetailInterface,
		const FAddPropertyParams& Params = FAddPropertyParams());

private:
	const TSharedRef<IPropertyHandle> InstancedStructHandle;
	const TSharedRef<FVoxelInstancedStructDataProvider> DataProvider;

	mutable uint64 LastPostChangeFrame = -1;

	FVoxelInstancedStructDetailsWrapper(
		const TSharedRef<IPropertyHandle>& InstancedStructHandle,
		const TSharedRef<FVoxelInstancedStructDataProvider>& DataProvider)
		: InstancedStructHandle(InstancedStructHandle)
		, DataProvider(DataProvider)
	{
	}

	void SetupChildHandle(const TSharedPtr<IPropertyHandle>& Handle) const;

	friend class FVoxelStructCustomizationWrapperTicker;
};