// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTransformRefImpl.h"
#include "VoxelDependency.h"
#include "VoxelTransformRefManager.h"

FVoxelTransformRefNodeArray::FVoxelTransformRefNodeArray(const TConstVoxelArrayView<FVoxelTransformRefNode> Nodes)
	: Nodes(Nodes)
{
	ensure(Nodes.Num() > 0);

	TVoxelInlineArray<uint32, 4> Hashes;
	Hashes.Add(Nodes.Num());
	for (const FVoxelTransformRefNode& Node : Nodes)
	{
		Hashes.Add(GetTypeHash(Node));
	}

	Hash = FVoxelUtilities::MurmurHashBytes(MakeByteVoxelArrayView(Hashes));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTransformRefImpl::FVoxelTransformRefImpl(const TConstVoxelArrayView<FVoxelTransformRefNode> Nodes)
	: Name(INLINE_LAMBDA
	{
		ensure(Nodes.Num() > 0);

		FString Result;
		for (const FVoxelTransformRefNode& Node : Nodes)
		{
			if (!Result.IsEmpty())
			{
				Result += " -> ";
			}
			Result += Node.DebugName.ToString();
		}
		return FName(Result);
	})
	, Dependency(FVoxelDependency::Create("TransformRef " + Name.ToString()))
	, Nodes(Nodes)
{
	ensure(Nodes.Num() > 0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelTransformRefImpl::TryInitialize_AnyThread()
{
	VOXEL_FUNCTION_COUNTER();
	ensure(!IsInGameThread());

	FMatrix NewTransform = FMatrix::Identity;
	for (const FVoxelTransformRefNode& Node : Nodes)
	{
		FVoxelTransformRefNode CanonNode = Node;
		CanonNode.bIsInverted = false;
		FVoxelTransformRefNodeArray NodeArray({ CanonNode });

		const TSharedPtr<FVoxelTransformRefImpl> TransformRef = GVoxelTransformRefManager->Find_AnyThread_RequiresLock(NodeArray);
		if (!ensure(TransformRef))
		{
			// Failed
			return;
		}

		const FMatrix LocalTransform = TransformRef->GetTransform();

		if (Node.bIsInverted)
		{
			NewTransform *= LocalTransform.Inverse();
		}
		else
		{
			NewTransform *= LocalTransform;
		}
	}
	Transform = NewTransform;
}

void FVoxelTransformRefImpl::Update_GameThread()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	FMatrix NewTransform = FMatrix::Identity;
	for (const FVoxelTransformRefNode& Node : Nodes)
	{
		FMatrix LocalTransform;
		if (Node.Provider.IsConstant())
		{
			LocalTransform = Node.Provider.GetLocalToWorld();
		}
		else
		{
			const USceneComponent* Component = Node.Provider.GetWeakComponent().Get();
			if (!/*ensureVoxelSlow*/(Component))
			{
				continue;
			}

			LocalTransform = Component->GetComponentTransform().ToMatrixWithScale();
		}

		if (Node.bIsInverted)
		{
			LocalTransform = LocalTransform.Inverse();
		}

		NewTransform *= LocalTransform;
	}

	if (Transform.Equals(NewTransform))
	{
		return;
	}

	Transform = NewTransform;
	Dependency->Invalidate();

	TVoxelArray<TSharedPtr<const FOnChanged>> OnChangedDelegates;
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		for (auto It = OnChangedDelegates_RequiresLock.CreateIterator(); It; ++It)
		{
			const TSharedPtr<const FOnChanged>& OnChanged = *It;
			if (!OnChanged->IsBound())
			{
				It.RemoveCurrentSwap();
				continue;
			}

			OnChangedDelegates.Add(OnChanged);
		}
	}

	for (const TSharedPtr<const FOnChanged>& OnChanged : OnChangedDelegates)
	{
		VOXEL_SCOPE_COUNTER("OnChanged");
		(void)OnChanged->ExecuteIfBound(Transform);
	}
}

void FVoxelTransformRefImpl::AddOnChanged(const FOnChanged& OnChanged)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	OnChangedDelegates_RequiresLock.Add(MakeSharedCopy(OnChanged));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<FVoxelTransformRefImpl> FVoxelTransformRefImpl::Multiply_AnyThread(
	const FVoxelTransformRefImpl& Other,
	const bool bIsInverted,
	const bool bOtherIsInverted) const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelInlineArray<FVoxelTransformRefNode, 8> NewNodes;
	NewNodes.Reserve(NewNodes.Num() + Other.Nodes.Num());

	for (FVoxelTransformRefNode Node : Nodes)
	{
		if (bIsInverted)
		{
			Node.bIsInverted = !Node.bIsInverted;
		}
		NewNodes.Add_CheckNoGrow(Node);
	}

	for (FVoxelTransformRefNode Node : Other.Nodes)
	{
		if (bOtherIsInverted)
		{
			Node.bIsInverted = !Node.bIsInverted;
		}

		if (NewNodes.Num() > 0 &&
			NewNodes.Last().IsInverseOf(Node))
		{
			NewNodes.Pop();
			continue;
		}

		NewNodes.Add_CheckNoGrow(Node);
	}

	if (NewNodes.Num() == 0)
	{
		return {};
	}

	return GVoxelTransformRefManager->Make_AnyThread(NewNodes);
}