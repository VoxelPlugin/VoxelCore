// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelAABBTree.h"
#include "VoxelWelfordVariance.h"

void FVoxelAABBTree::Initialize(TVoxelArray<FElement>&& InElements)
{
	VOXEL_FUNCTION_COUNTER_NUM(InElements.Num(), 128);
	check(Nodes.Num() == 0);
	check(Leaves.Num() == 0);

	if (InElements.Num() == 0)
	{
		return;
	}

#if VOXEL_DEBUG
	for (const FElement& Element : InElements)
	{
		ensure(Element.Bounds.IsValid());
	}
#endif

	const int32 NumElements = InElements.Num();
	const int32 ExpectedNumLeaves = 2 * FVoxelUtilities::DivideCeil(NumElements, MaxChildrenInLeaf);
	const int32 ExpectedNumNodes = 2 * ExpectedNumLeaves;

	Nodes.Reserve(ExpectedNumNodes);
	Leaves.Reserve(ExpectedNumLeaves);

	struct FNodeToProcess
	{
		FVoxelBox Bounds;
		TVoxelArray<FElement> Elements;
		TVoxelWelfordVariance<FVector> CenterWelfordVariance;

		int32 NodeLevel = -1;
		int32 NodeIndex = -1;

		void Reset()
		{
			Bounds = {};
			Elements.Reset();
			CenterWelfordVariance = {};
			NodeLevel = -1;
			NodeIndex = -1;
		}
	};

	TVoxelArray<TVoxelUniquePtr<FNodeToProcess>> NodesToProcess;
	NodesToProcess.Reserve(ExpectedNumNodes);

	// Create root node
	{
		TVoxelUniquePtr<FNodeToProcess> NodeToProcess = MakeVoxelUnique<FNodeToProcess>();
		NodeToProcess->Elements = MoveTemp(InElements);
		NodeToProcess->Bounds = FVoxelBox::InvertedInfinite;
		for (const FElement& Element : NodeToProcess->Elements)
		{
			NodeToProcess->Bounds += Element.Bounds;
			NodeToProcess->CenterWelfordVariance.Add(Element.Bounds.GetCenter());
		}

		NodeToProcess->NodeLevel = 0;
		NodeToProcess->NodeIndex = Nodes.Emplace();

		RootBounds = NodeToProcess->Bounds;

		NodesToProcess.Add(MoveTemp(NodeToProcess));
	}

	TVoxelArray<TVoxelUniquePtr<FNodeToProcess>> PooledNodesToProcess;
	const auto AllocateNodeToProcess = [&]
	{
		if (PooledNodesToProcess.Num() > 0)
		{
			return PooledNodesToProcess.Pop();
		}
		return MakeVoxelUnique<FNodeToProcess>();
	};

	while (NodesToProcess.Num())
	{
		TVoxelUniquePtr<FNodeToProcess> NodeToProcess = NodesToProcess.Pop();
		ON_SCOPE_EXIT
		{
			NodeToProcess->Reset();
			PooledNodesToProcess.Add(MoveTemp(NodeToProcess));
		};

		Nodes.Reserve(Nodes.Num() + 2);

		// Check Node will not be invalidated
		const int32 CurrentNodesMax = Nodes.Max();
		ON_SCOPE_EXIT
		{
			checkVoxelSlow(CurrentNodesMax == Nodes.Max());
		};

		FNode& Node = Nodes[NodeToProcess->NodeIndex];

		if (NodeToProcess->Elements.Num() <= MaxChildrenInLeaf ||
			NodeToProcess->NodeLevel >= MaxTreeDepth)
		{
			Node.bLeaf = true;
			Node.LeafIndex = Leaves.Add(FLeaf{ MoveTemp(NodeToProcess->Elements) });
			continue;
		}

		TVoxelUniquePtr<FNodeToProcess> ChildToProcess0 = AllocateNodeToProcess();
		TVoxelUniquePtr<FNodeToProcess> ChildToProcess1 = AllocateNodeToProcess();

		// Split on max center variance
		// Could also split on max bound size, but variance should lead to better results
		const int32 MaxAxis = FVoxelUtilities::GetLargestAxis(NodeToProcess->CenterWelfordVariance.GetVariance());

		// Could also split at bounds center
		const FVector SplitCenter = NodeToProcess->CenterWelfordVariance.Average;

		ChildToProcess0->Bounds = FVoxelBox::InvertedInfinite;
		ChildToProcess1->Bounds = FVoxelBox::InvertedInfinite;
		ensure(ChildToProcess0->Elements.Num() == 0);
		ensure(ChildToProcess1->Elements.Num() == 0);
		ChildToProcess0->Elements.Reserve(NodeToProcess->Elements.Num() * 1.5f);
		ChildToProcess1->Elements.Reserve(NodeToProcess->Elements.Num() * 1.5f);

		const double SplitValue = SplitCenter[MaxAxis];
		for (const FElement& Element : NodeToProcess->Elements)
		{
			const FVector ElementCenter = Element.Bounds.GetCenter();
			const double ElementValue = ElementCenter[MaxAxis];

			if (ElementValue <= SplitValue)
			{
				ChildToProcess0->Bounds += Element.Bounds;
				ChildToProcess0->Elements.Add(Element);
				ChildToProcess0->CenterWelfordVariance.Add(ElementCenter);
			}
			else
			{
				ChildToProcess1->Bounds += Element.Bounds;
				ChildToProcess1->Elements.Add(Element);
				ChildToProcess1->CenterWelfordVariance.Add(ElementCenter);
			}
		}

		// Failed to split
		if (ChildToProcess0->Elements.Num() == 0 ||
			ChildToProcess1->Elements.Num() == 0)
		{
#if VOXEL_DEBUG
			TVoxelSet<FVoxelBox> Elements0;
			TVoxelSet<FVoxelBox> Elements1;
			for (const FElement& Element : ChildToProcess0->Elements)
			{
				Elements0.Add(Element.Bounds);
			}
			for (const FElement& Element : ChildToProcess1->Elements)
			{
				Elements1.Add(Element.Bounds);
			}
			ensure(Elements0.Num() == 1 || Elements1.Num() == 1);
#endif

			Node.bLeaf = true;
			Node.LeafIndex = Leaves.Add(FLeaf{ MoveTemp(NodeToProcess->Elements) });
			continue;
		}

		ChildToProcess0->NodeIndex = Nodes.Emplace();
		ChildToProcess1->NodeIndex = Nodes.Emplace();

		Node.bLeaf = false;
		Node.ChildBounds0 = ChildToProcess0->Bounds;
		Node.ChildBounds1 = ChildToProcess1->Bounds;
		Node.ChildIndex0 = ChildToProcess0->NodeIndex;
		Node.ChildIndex1 = ChildToProcess1->NodeIndex;

		NodesToProcess.Add(MoveTemp(ChildToProcess0));
		NodesToProcess.Add(MoveTemp(ChildToProcess1));
	}

#if VOXEL_DEBUG
	int32 NumElementsInLeaves = 0;
	for (const FLeaf& Leaf : Leaves)
	{
		NumElementsInLeaves += Leaf.Elements.Num();
	}
	ensure(NumElementsInLeaves == NumElements);
#endif
}

void FVoxelAABBTree::Shrink()
{
	VOXEL_FUNCTION_COUNTER();

	Nodes.Shrink();
	Leaves.Shrink();
}

TSharedRef<FVoxelAABBTree> FVoxelAABBTree::Create(const TConstVoxelArrayView<FVoxelBox> Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<FElement> Elements;
	FVoxelUtilities::SetNumFast(Elements, Bounds.Num());

	for (int32 Index = 0; Index < Bounds.Num(); Index++)
	{
		Elements[Index] = FElement
		{
			Bounds[Index],
			Index
		};
	}

	const TSharedRef<FVoxelAABBTree> Tree = MakeVoxelShared<FVoxelAABBTree>();
	Tree->Initialize(MoveTemp(Elements));
	return Tree;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAABBTree::BulkRaycast(
	const TConstVoxelArrayView<FVector3f> InRayPositions,
	const TConstVoxelArrayView<FVector3f> InRayDirections,
	const FBulkRaycastLambda Lambda)
{
	VOXEL_FUNCTION_COUNTER();
	check(InRayPositions.Num() == InRayDirections.Num());

	if (Nodes.Num() == 0)
	{
		return;
	}

	struct FCachedArray
	{
		TVoxelArray<FVector3f> RayPositions;
		TVoxelArray<FVector3f> RayDirections;

		int32 Num() const
		{
			check(RayPositions.Num() == RayDirections.Num());
			return RayPositions.Num();
		}

		void Initialize(
			const FCachedArray& Parent,
			const FVoxelBox& Bounds)
		{
			VOXEL_FUNCTION_COUNTER();
			const int32 ParentNum = Parent.Num();

			RayPositions.Reserve(ParentNum);
			RayDirections.Reserve(ParentNum);

			for (int32 Index = 0; Index < ParentNum; Index++)
			{
				const FVector3f RayPosition = Parent.RayPositions[Index];
				const FVector3f RayDirection = Parent.RayDirections[Index];
				ensureVoxelSlowNoSideEffects(RayDirection.IsNormalized());

				if (!Bounds.RayBoxIntersection(FVector(RayPosition), FVector(RayDirection)))
				{
					continue;
				}

				RayPositions.Add(RayPosition);
				RayDirections.Add(RayDirection);
			}
		}
	};
	TVoxelArray<TUniquePtr<FCachedArray>> PooledCachedArrays;

	const auto AllocateCachedArray = [&]
	{
		if (PooledCachedArrays.Num() > 0)
		{
			return PooledCachedArrays.Pop();
		}
		return MakeUnique<FCachedArray>();
	};
	const auto ReturnToPool = [&](TUniquePtr<FCachedArray>& CachedArray)
	{
		CachedArray->RayPositions.Reset();
		CachedArray->RayDirections.Reset();
		PooledCachedArrays.Add(MoveTemp(CachedArray));
	};

	struct FQueuedNode
	{
		int32 NodeIndex = -1;
		TUniquePtr<FCachedArray> CachedArray;
	};
	TVoxelInlineArray<FQueuedNode, 64> QueuedNodes;

	{
		VOXEL_SCOPE_COUNTER("First copy");

		TUniquePtr<FCachedArray> CachedArray = AllocateCachedArray();
		CachedArray->RayPositions = TVoxelArray<FVector3f>(InRayPositions);
		CachedArray->RayDirections = TVoxelArray<FVector3f>(InRayDirections);

		QueuedNodes.Add(FQueuedNode
		{
			0,
			MoveTemp(CachedArray)
		});
	}

	while (QueuedNodes.Num() > 0)
	{
		FQueuedNode QueuedNode = QueuedNodes.Pop();
		ON_SCOPE_EXIT
		{
			ReturnToPool(QueuedNode.CachedArray);
		};

		const FNode& Node = Nodes[QueuedNode.NodeIndex];
		if (Node.bLeaf)
		{
			const FLeaf& Leaf = Leaves[Node.LeafIndex];
			for (const FElement& Element : Leaf.Elements)
			{
				TUniquePtr<FCachedArray> CachedArray = AllocateCachedArray();
				ON_SCOPE_EXIT
				{
					ReturnToPool(CachedArray);
				};
				CachedArray->Initialize(*QueuedNode.CachedArray, Element.Bounds);

				if (CachedArray->Num() == 0)
				{
					continue;
				}

				Lambda(
					Element.Payload,
					CachedArray->RayPositions,
					CachedArray->RayDirections);
			}
		}
		else
		{
			{
				TUniquePtr<FCachedArray> CachedArray = AllocateCachedArray();
				CachedArray->Initialize(*QueuedNode.CachedArray, Node.ChildBounds0);
				if (CachedArray->Num() == 0)
				{
					ReturnToPool(CachedArray);
				}
				else
				{
					QueuedNodes.Add({ Node.ChildIndex0, MoveTemp(CachedArray) });
				}
			}
			{
				TUniquePtr<FCachedArray> CachedArray = AllocateCachedArray();
				CachedArray->Initialize(*QueuedNode.CachedArray, Node.ChildBounds1);
				if (CachedArray->Num() == 0)
				{
					ReturnToPool(CachedArray);
				}
				else
				{
					QueuedNodes.Add({ Node.ChildIndex1, MoveTemp(CachedArray) });
				}
			}
		}
	}
}