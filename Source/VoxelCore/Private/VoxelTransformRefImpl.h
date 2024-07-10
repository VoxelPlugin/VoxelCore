// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependency;

struct VOXELCORE_API FVoxelTransformRefNode
{
	FObjectKey WeakComponent;
	bool bIsInverted = false;
	FName DebugName;

	explicit FVoxelTransformRefNode(const USceneComponent& Component)
		: WeakComponent(&Component)
	{
		checkUObjectAccess();

		DebugName = FName(Component.GetReadableName());
	}

	FORCEINLINE const USceneComponent* GetComponent() const
	{
		checkVoxelSlow(IsInGameThread());

		UObject* Object = WeakComponent.ResolveObjectPtr();
		if (!Object)
		{
			return nullptr;
		}
		return CastChecked<USceneComponent>(Object);
	}
	FORCEINLINE bool IsInverseOf(const FVoxelTransformRefNode& Other) const
	{
		return
			bIsInverted != Other.bIsInverted &&
			WeakComponent == Other.WeakComponent;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelTransformRefNodeArray
{
public:
	FVoxelTransformRefNodeArray() = default;
	explicit FVoxelTransformRefNodeArray(TConstVoxelArrayView<FVoxelTransformRefNode> Nodes);

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelTransformRefNodeArray& NodeArray)
	{
		checkVoxelSlow(NodeArray.Hash != 0);
		return NodeArray.Hash;
	}
	FORCEINLINE bool operator==(const FVoxelTransformRefNodeArray& Other) const
	{
		return
			WeakComponents == Other.WeakComponents &&
			IsInverted == Other.IsInverted;
	}

private:
	uint64 Hash = 0;
	TVoxelInlineArray<FObjectKey, 4> WeakComponents;
	TVoxelBitArray<TVoxelInlineAllocator<1>> IsInverted;

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VOXELCORE_API FVoxelTransformRefImpl : public TSharedFromThis<FVoxelTransformRefImpl>
{
public:
	const FName Name;
	const TSharedRef<FVoxelDependency> Dependency;
	const TVoxelArray<FVoxelTransformRefNode> Nodes;

	using FOnChanged = TDelegate<void(const FMatrix& NewTransform)>;

	explicit FVoxelTransformRefImpl(TConstVoxelArrayView<FVoxelTransformRefNode> Nodes);

	FORCEINLINE const FMatrix& GetTransform() const
	{
		return Transform;
	}

	void TryInitialize_AnyThread();
	void Update_GameThread();
	void AddOnChanged(const FOnChanged& OnChanged);

	TSharedPtr<FVoxelTransformRefImpl> Multiply_AnyThread(
		const FVoxelTransformRefImpl& Other,
		bool bIsInverted,
		bool bOtherIsInverted) const;

private:
	FMatrix Transform = FMatrix::Identity;

	FVoxelCriticalSection CriticalSection;
	TVoxelArray<TSharedPtr<const FOnChanged>> OnChangedDelegates_RequiresLock;
};