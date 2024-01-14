// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class FVoxelDetailInterface;

class VOXELCOREEDITOR_API FVoxelStructDetailsWrapper : public TSharedFromThis<FVoxelStructDetailsWrapper>
{
public:
	using FGetStructView = TFunction<FConstVoxelStructView(const UObject&)>;
	using FSetStructView = TFunction<void(UObject&, FConstVoxelStructView)>;

	TMap<FName, FString> InstanceMetadataMap;

	static TSharedRef<FVoxelStructDetailsWrapper> Make(
		const TArray<TWeakObjectPtr<UObject>>& WeakObjects,
		const UScriptStruct& ScriptStruct,
		const FGetStructView& GetStructView,
		const FSetStructView& SetStructView);

	template<typename ObjectType, typename StructType>
	static TSharedRef<FVoxelStructDetailsWrapper> Make(
		const TArray<TWeakObjectPtr<ObjectType>>& WeakObjects,
		const TFunction<const StructType*(const ObjectType&)>& GetStruct,
		const TFunction<void(ObjectType&, const StructType&)>& SetStruct)
	{
		return FVoxelStructDetailsWrapper::Make(
			TArray<TWeakObjectPtr<UObject>>(WeakObjects),
			*StaticStructFast<StructType>(),
			[=](const UObject& Object) -> FConstVoxelStructView
			{
				const StructType* Struct = GetStruct(*CastChecked<ObjectType>(&Object));
				if (!ensureVoxelSlow(Struct))
				{
					return {};
				}
				return MakeVoxelStructView(*Struct);
			},
			[=](UObject& Object, const FConstVoxelStructView NewStructView)
			{
				SetStruct(*CastChecked<ObjectType>(&Object), NewStructView.Get<StructType>());
			});
	}
	template<typename ObjectType, typename StructType>
	static TSharedRef<FVoxelStructDetailsWrapper> Make(
		IDetailLayoutBuilder& DetailLayout,
		const TFunction<const StructType*(const ObjectType&)>& GetStruct,
		const TFunction<void(ObjectType&, const StructType&)>& SetStruct)
	{
		TArray<TWeakObjectPtr<UObject>> WeakObjects;
		DetailLayout.GetObjectsBeingCustomized(WeakObjects);

		TArray<TWeakObjectPtr<ObjectType>> TypedWeakObjects;
		for (const TWeakObjectPtr<UObject>& WeakObject : WeakObjects)
		{
			if (!ensureVoxelSlow(WeakObject.IsValid()))
			{
				continue;
			}

			ObjectType* TypedObject = Cast<ObjectType>(WeakObject.Get());
			if (!TypedObject)
			{
				// Useful if eg node is selected and we need a graph
				TypedObject = WeakObject->GetTypedOuter<ObjectType>();
				if (!ensure(TypedObject))
				{
					continue;
				}
			}

			TypedWeakObjects.Add(TypedObject);
		}

		return FVoxelStructDetailsWrapper::Make<ObjectType, StructType>(
			TypedWeakObjects,
			GetStruct,
			SetStruct);
	}

	VOXEL_COUNT_INSTANCES();

	void AddChildrenTo(const FVoxelDetailInterface& DetailInterface) const;

private:
	const TSharedRef<FStructOnScope> StructOnScope;
	const TArray<TWeakObjectPtr<UObject>> WeakObjects;
	const FGetStructView GetStructView;
	const FSetStructView SetStructView;

	double LastSyncTime = 0.0;
	mutable uint64 LastPostChangeFrame = -1;

	FVoxelStructDetailsWrapper(
		const TSharedRef<FStructOnScope>& StructOnScope,
		const TArray<TWeakObjectPtr<UObject>>& WeakObjects,
		const FGetStructView& GetStructView,
		const FSetStructView& SetStructView)
		: StructOnScope(StructOnScope)
		, WeakObjects(WeakObjects)
		, GetStructView(GetStructView)
		, SetStructView(SetStructView)
	{
	}

	void SyncFromSource() const;
	void SyncToSource() const;
	void SetupChildHandle(const TSharedRef<IPropertyHandle>& Handle) const;

	friend class FVoxelStructDetailsWrapperTicker;
};