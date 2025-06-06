// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "Details/VoxelInstancedStructDataProvider.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelInstancedStructDetailsWrapper);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelInstancedStructDetailsWrapper> FVoxelInstancedStructDetailsWrapper::Make(const TSharedRef<IPropertyHandle>& InstancedStructHandle)
{
	FVoxelEditorUtilities::TrackHandle(InstancedStructHandle);

	if (!ensure(InstancedStructHandle->IsValidHandle()))
	{
		return nullptr;
	}

	const TSharedRef<FVoxelInstancedStructDataProvider> DataProvider = MakeShared<FVoxelInstancedStructDataProvider>(InstancedStructHandle);

	return MakeShareable(new FVoxelInstancedStructDetailsWrapper(InstancedStructHandle, DataProvider));
}

TArray<TSharedPtr<IPropertyHandle>> FVoxelInstancedStructDetailsWrapper::AddChildStructure()
{
	const TArray<TSharedPtr<IPropertyHandle>> ChildHandles = InstancedStructHandle->AddChildStructure(DataProvider);
	for (const TSharedPtr<IPropertyHandle>& ChildHandle : ChildHandles)
	{
		SetupChildHandle(ChildHandle);
	}
	return ChildHandles;
}

IDetailPropertyRow* FVoxelInstancedStructDetailsWrapper::AddExternalStructure(
	const FVoxelDetailInterface& DetailInterface,
	const FAddPropertyParams& Params)
{
	const TSharedRef<FVoxelInstancedStructDataProvider> DataProviderCopy = MakeSharedCopy(*DataProvider);
	// Otherwise FStructurePropertyNode::GetInstancesNum will crash
	DataProviderCopy->bIsPropertyIndirection = false;

	FAddPropertyParams NewParams = Params;
#if VOXEL_ENGINE_VERSION >= 506
	// When creating category nodes, FProperty to new property handle won't be assigned and it will crash
	NewParams.CreateCategoryNodes(false);
#endif

	IDetailPropertyRow* Row = DetailInterface.AddExternalStructure(DataProviderCopy, NewParams);
	if (!ensure(Row))
	{
		return nullptr;
	}

	for (const TSharedRef<IPropertyHandle>& ChildHandle : FVoxelEditorUtilities::GetChildHandles(Row->GetPropertyHandle(), true, true))
	{
		SetupChildHandle(ChildHandle);
	}

	return Row;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedStructDetailsWrapper::SetupChildHandle(const TSharedPtr<IPropertyHandle>& Handle) const
{
	if (!ensure(Handle))
	{
		return;
	}

	// Forward only direct parent all meta data
	if (const TMap<FName, FString>* Map = InstancedStructHandle->GetInstanceMetaDataMap())
	{
		for (auto& It : *Map)
		{
			Handle->SetInstanceMetaData(It.Key, It.Value);
		}
	}

	const FSimpleDelegate PreChangeDelegate = MakeWeakPtrDelegate(this, [this]
	{
		VOXEL_SCOPE_COUNTER("NotifyPreChange");
		InstancedStructHandle->NotifyPreChange();
	});
	const TDelegate<void(const FPropertyChangedEvent&)> PostChangeDelegate = MakeWeakPtrDelegate(this, [this](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		// Critical to not have an exponential number of PostChange fired
		// InstancedStructHandle->NotifyPostChange will call the PostChangeDelegates of child struct customization
		if (LastPostChangeFrame == GFrameCounter &&
			PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
		{
			return;
		}
		LastPostChangeFrame = GFrameCounter;

		{
			VOXEL_SCOPE_COUNTER("NotifyPostChange");
			InstancedStructHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
		}

		if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			VOXEL_SCOPE_COUNTER("NotifyFinishedChangingProperties");
			InstancedStructHandle->NotifyFinishedChangingProperties();
		}
	});

	Handle->SetOnPropertyValuePreChange(PreChangeDelegate);
	Handle->SetOnPropertyValueChangedWithData(PostChangeDelegate);

	Handle->SetOnChildPropertyValuePreChange(PreChangeDelegate);
	Handle->SetOnChildPropertyValueChangedWithData(PostChangeDelegate);
}