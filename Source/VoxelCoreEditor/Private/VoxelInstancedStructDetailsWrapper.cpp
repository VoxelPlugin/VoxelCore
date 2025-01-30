// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelInstancedStructDetailsWrapper);

class FVoxelStructCustomizationWrapperTicker : public FVoxelEditorSingleton
{
public:
	TArray<TWeakPtr<FVoxelInstancedStructDetailsWrapper>> WeakWrappers;

	//~ Begin FVoxelEditorSingleton Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		WeakWrappers.RemoveAllSwap([](const TWeakPtr<FVoxelInstancedStructDetailsWrapper>& WeakChildWrapper)
		{
			return !WeakChildWrapper.IsValid();
		});

		const double Time = FPlatformTime::Seconds();
		for (const TWeakPtr<FVoxelInstancedStructDetailsWrapper>& WeakWrapper : WeakWrappers)
		{
			const TSharedPtr<FVoxelInstancedStructDetailsWrapper> Wrapper = WeakWrapper.Pin();
			if (!ensure(Wrapper.IsValid()))
			{
				continue;
			}

			if (Time < Wrapper->LastSyncTime + 0.1)
			{
				continue;
			}
			Wrapper->LastSyncTime = Time;

			// Tricky: can tick once after the property is gone due to SListPanel being delayed
			Wrapper->SyncFromSource();
		}
	}
	//~ End FVoxelEditorSingleton Interface
};
FVoxelStructCustomizationWrapperTicker* GVoxelStructCustomizationWrapperTicker = new FVoxelStructCustomizationWrapperTicker();

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

	bool bHasValidStruct = true;
	TOptional<const UScriptStruct*> Struct;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(InstancedStructHandle, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		if (!Struct.IsSet())
		{
			Struct = InstancedStruct.GetScriptStruct();
			return;
		}

		if (Struct.GetValue() != InstancedStruct.GetScriptStruct())
		{
			bHasValidStruct = false;
		}
	});

	if (!ensure(Struct.IsSet()) ||
		!bHasValidStruct ||
		!Struct.GetValue())
	{
		return nullptr;
	}

	const TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(Struct.GetValue());

	// Make sure the struct also has a valid package set, so that properties that rely on this (like FText) work correctly
	{
		TArray<UPackage*> OuterPackages;
		InstancedStructHandle->GetOuterPackages(OuterPackages);
		if (OuterPackages.Num() > 0)
		{
			StructOnScope->SetPackage(OuterPackages[0]);
		}
	}

	const TSharedRef<FVoxelInstancedStructDetailsWrapper> Result(new FVoxelInstancedStructDetailsWrapper(InstancedStructHandle, StructOnScope));
	GVoxelStructCustomizationWrapperTicker->WeakWrappers.Add(Result);
	Result->SyncFromSource();
	return Result;
}

TArray<TSharedPtr<IPropertyHandle>> FVoxelInstancedStructDetailsWrapper::AddChildStructure()
{
	const TArray<TSharedPtr<IPropertyHandle>> ChildHandles = InstancedStructHandle->AddChildStructure(StructOnScope);
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
	IDetailPropertyRow* Row = DetailInterface.AddExternalStructure(StructOnScope, Params);
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

void FVoxelInstancedStructDetailsWrapper::SyncFromSource() const
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(InstancedStructHandle, [&](FVoxelInstancedStruct& InstancedStruct)
	{
		if (!ensureVoxelSlow(InstancedStruct.GetScriptStruct() == StructOnScope->GetStruct()))
		{
			return;
		}

		FVoxelStructView(InstancedStruct).CopyTo(*StructOnScope);
	});
}

void FVoxelInstancedStructDetailsWrapper::SyncToSource() const
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(InstancedStructHandle, [&](FVoxelInstancedStruct& InstancedStruct)
	{
		if (!ensureVoxelSlow(InstancedStruct.GetScriptStruct() == StructOnScope->GetStruct()))
		{
			return;
		}

		FVoxelStructView(*StructOnScope).CopyTo(InstancedStruct);
	});
}

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

	// Forward instance metadata, used to avoid infinite recursion in inline graphs
	for (TSharedPtr<IPropertyHandle> ParentHandle = InstancedStructHandle; ParentHandle; ParentHandle = ParentHandle->GetParentHandle())
	{
		if (const TMap<FName, FString>* Map = ParentHandle->GetInstanceMetaDataMap())
		{
			for (auto& It : *Map)
			{
				if (It.Key.ToString().StartsWith("Recursive_"))
				{
					Handle->SetInstanceMetaData(It.Key, It.Value);
				}
			}
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

		SyncToSource();

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