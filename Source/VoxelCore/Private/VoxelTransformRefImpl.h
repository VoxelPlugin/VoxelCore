// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class FVoxelDependency;

class FVoxelTransformRefProvider
{
public:
	explicit FVoxelTransformRefProvider(const FMatrix& LocalToWorld)
		: bIsConstant(true)
		, LocalToWorld(LocalToWorld)
		, Component({})
	{
	}
	explicit FVoxelTransformRefProvider(const USceneComponent& Component)
		: bIsConstant(false)
		, LocalToWorld({})
		, Component(&Component)
	{
	}

	FORCEINLINE bool IsConstant() const
	{
		return bIsConstant;
	}
	FORCEINLINE const FMatrix& GetLocalToWorld() const
	{
		checkVoxelSlow(IsConstant());
		return LocalToWorld;
	}
	FORCEINLINE const TObjectKey<USceneComponent>& GetComponent() const
	{
		checkVoxelSlow(!IsConstant());
		return Component;
	}

	FORCEINLINE bool operator==(const FVoxelTransformRefProvider& Other) const
	{
		if (bIsConstant != Other.bIsConstant)
		{
			return false;
		}

		if (bIsConstant)
		{
			return LocalToWorld == Other.LocalToWorld;
		}

		return Component == Other.Component;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelTransformRefProvider& Provider)
	{
		if (Provider.bIsConstant)
		{
			return FVoxelUtilities::MurmurHash(Provider.LocalToWorld);
		}

		return GetTypeHash(Provider.Component);
	}

private:
	const bool bIsConstant;
	const FMatrix LocalToWorld;
	const TObjectKey<USceneComponent> Component;
};

struct FVoxelTransformRefNode
{
	FVoxelTransformRefProvider Provider;
	bool bIsInverted = false;
	FName DebugName;

	explicit FVoxelTransformRefNode(const FVoxelTransformRefProvider& Provider)
		: Provider(Provider)
	{
		checkUObjectAccess();

		if (Provider.IsConstant())
		{
			DebugName = STATIC_FNAME("Constant");
			return;
		}

		const USceneComponent* Component = Provider.GetComponent().ResolveObjectPtr();
		if (!ensureVoxelSlow(Component))
		{
			return;
		}

		DebugName = FName(Component->GetReadableName());
	}

	FORCEINLINE bool IsInverseOf(const FVoxelTransformRefNode& Other) const
	{
		return
			bIsInverted != Other.bIsInverted &&
			Provider == Other.Provider;
	}

	FORCEINLINE bool operator==(const FVoxelTransformRefNode& Other) const
	{
		return
			Provider == Other.Provider &&
			bIsInverted == Other.bIsInverted;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelTransformRefNode& Node)
	{
		return
			GetTypeHash(Node.Provider) ^
			GetTypeHash(Node.bIsInverted);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelTransformRefNodeArray
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
		return Nodes == Other.Nodes;
	}

private:
	uint32 Hash = 0;
	TVoxelInlineArray<FVoxelTransformRefNode, 2> Nodes;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelTransformRefImpl : public TSharedFromThis<FVoxelTransformRefImpl>
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