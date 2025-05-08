// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelStructDetailsWrapper);

class FVoxelStructDetailsWrapperTicker : public FVoxelEditorSingleton
{
public:
	TArray<TWeakPtr<FVoxelStructDetailsWrapper>> WeakWrappers;

	//~ Begin FVoxelEditorSingleton Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		WeakWrappers.RemoveAllSwap([](const TWeakPtr<FVoxelStructDetailsWrapper>& WeakChildWrapper)
		{
			return !WeakChildWrapper.IsValid();
		});

		const double Time = FPlatformTime::Seconds();
		for (const TWeakPtr<FVoxelStructDetailsWrapper>& WeakWrapper : WeakWrappers)
		{
			const TSharedPtr<FVoxelStructDetailsWrapper> Wrapper = WeakWrapper.Pin();
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
FVoxelStructDetailsWrapperTicker* GVoxelStructDetailsWrapperTicker = new FVoxelStructDetailsWrapperTicker();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelStructDetailsWrapper> FVoxelStructDetailsWrapper::Make(
	const TArray<TVoxelObjectPtr<UObject>>& WeakObjects,
	const UScriptStruct& ScriptStruct,
	const FGetStructView& GetStructView,
	const FSetStructView& SetStructView)
{
	const TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(&ScriptStruct);

	// Make sure the struct also has a valid package set, so that properties that rely on this (like FText) work correctly
	{
		for (const TVoxelObjectPtr<UObject>& WeakObject : WeakObjects)
		{
			const UObject* Object = WeakObject.Resolve();
			if (!ensureVoxelSlow(Object))
			{
				continue;
			}

			StructOnScope->SetPackage(Object->GetPackage());
		}
	}

	const TSharedRef<FVoxelStructDetailsWrapper> Result(new FVoxelStructDetailsWrapper(
		StructOnScope,
		WeakObjects,
		GetStructView,
		SetStructView));
	GVoxelStructDetailsWrapperTicker->WeakWrappers.Add(Result);
	Result->SyncFromSource();
	return Result;
}

void FVoxelStructDetailsWrapper::AddChildrenTo(const FVoxelDetailInterface& DetailInterface) const
{
	IDetailPropertyRow* Row = DetailInterface.AddExternalStructure(StructOnScope);
	if (!ensure(Row))
	{
		return;
	}

	for (const TSharedRef<IPropertyHandle>& ChildHandle : FVoxelEditorUtilities::GetChildHandles(Row->GetPropertyHandle(), true, true))
	{
		SetupChildHandle(ChildHandle);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelStructDetailsWrapper::SyncFromSource() const
{
	VOXEL_FUNCTION_COUNTER();

	for (const TVoxelObjectPtr<UObject>& WeakObject : WeakObjects)
	{
		const UObject* Object = WeakObject.Resolve();
		if (!ensureVoxelSlow(Object))
		{
			continue;
		}

		const FConstVoxelStructView StructView = GetStructView(*Object);
		const FVoxelStructView StructOnScopeView(*StructOnScope);
		if (!ensureVoxelSlow(StructView.IsValid()) ||
			!ensureVoxelSlow(StructView.GetScriptStruct() == StructOnScopeView.GetScriptStruct()))
		{
			continue;
		}

		StructView.CopyTo(*StructOnScope);
	}
}

void FVoxelStructDetailsWrapper::SyncToSource() const
{
	VOXEL_FUNCTION_COUNTER();

	for (const TVoxelObjectPtr<UObject>& WeakObject : WeakObjects)
	{
		UObject* Object = WeakObject.Resolve();
		if (!ensureVoxelSlow(Object))
		{
			continue;
		}

		const FConstVoxelStructView StructOnScopeView(*StructOnScope);
		SetStructView(*Object, StructOnScopeView);
	}
}

void FVoxelStructDetailsWrapper::SetupChildHandle(const TSharedRef<IPropertyHandle>& Handle) const
{
	for (const auto& It : InstanceMetadataMap)
	{
		Handle->SetInstanceMetaData(It.Key, It.Value);
	}

	const FSimpleDelegate PreChangeDelegate = MakeWeakPtrDelegate(this, [this]
	{
		VOXEL_SCOPE_COUNTER("PreEditChange");

		for (const TVoxelObjectPtr<UObject>& WeakObject : WeakObjects)
		{
			UObject* Object = WeakObject.Resolve();
			if (!ensureVoxelSlow(Object))
			{
				continue;
			}

			Object->PreEditChange(nullptr);
		}
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

		VOXEL_SCOPE_COUNTER("PostEditChangeProperty");

		for (const TVoxelObjectPtr<UObject>& WeakObject : WeakObjects)
		{
			UObject* Object = WeakObject.Resolve();
			if (!ensureVoxelSlow(Object))
			{
				continue;
			}

			FPropertyChangedEvent ObjectPropertyChangedEvent(nullptr, PropertyChangedEvent.ChangeType);
			Object->PostEditChangeProperty(ObjectPropertyChangedEvent);
		}
	});

	Handle->SetOnPropertyValuePreChange(PreChangeDelegate);
	Handle->SetOnPropertyValueChangedWithData(PostChangeDelegate);

	Handle->SetOnChildPropertyValuePreChange(PreChangeDelegate);
	Handle->SetOnChildPropertyValueChangedWithData(PostChangeDelegate);
}