// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelAABBTree2D.h"
#include "VoxelWelfordVariance.h"

void FVoxelAABBTree2D::Initialize(TVoxelArray<FElement>&& InElements)
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
		FVoxelBox2D Bounds;
		TVoxelArray<FElement> Elements;
		TVoxelWelfordVariance<FVector2D> CenterWelfordVariance;

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

	TVoxelArray<TUniquePtr<FNodeToProcess>> NodesToProcess;
	NodesToProcess.Reserve(ExpectedNumNodes);

	// Create root node
	{
		TUniquePtr<FNodeToProcess> NodeToProcess = MakeUnique<FNodeToProcess>();
		NodeToProcess->Elements = MoveTemp(InElements);
		NodeToProcess->Bounds = FVoxelBox2D::InvertedInfinite;
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

	TVoxelArray<TUniquePtr<FNodeToProcess>> PooledNodesToProcess;
	const auto AllocateNodeToProcess = [&]
	{
		if (PooledNodesToProcess.Num() > 0)
		{
			return PooledNodesToProcess.Pop();
		}
		return MakeUnique<FNodeToProcess>();
	};

	while (NodesToProcess.Num())
	{
		TUniquePtr<FNodeToProcess> NodeToProcess = NodesToProcess.Pop();
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
			check(CurrentNodesMax == Nodes.Max());
		};

		FNode& Node = Nodes[NodeToProcess->NodeIndex];

		if (NodeToProcess->Elements.Num() <= MaxChildrenInLeaf ||
			NodeToProcess->NodeLevel >= MaxTreeDepth)
		{
			Node.bLeaf = true;
			Node.LeafIndex = Leaves.Add(FLeaf{ MoveTemp(NodeToProcess->Elements) });
			continue;
		}

		TUniquePtr<FNodeToProcess> ChildToProcess0 = AllocateNodeToProcess();
		TUniquePtr<FNodeToProcess> ChildToProcess1 = AllocateNodeToProcess();

		// Split on max center variance
		// Could also split on max bound size, but variance should lead to better results
		const int32 MaxAxis = FVoxelUtilities::GetLargestAxis(NodeToProcess->CenterWelfordVariance.GetVariance());

		// Could also split at bounds center
		const FVector2D SplitCenter = NodeToProcess->CenterWelfordVariance.Average;

		ChildToProcess0->Bounds = FVoxelBox2D::InvertedInfinite;
		ChildToProcess1->Bounds = FVoxelBox2D::InvertedInfinite;
		ensure(ChildToProcess0->Elements.Num() == 0);
		ensure(ChildToProcess1->Elements.Num() == 0);
		ChildToProcess0->Elements.Reserve(NodeToProcess->Elements.Num() * 1.5f);
		ChildToProcess1->Elements.Reserve(NodeToProcess->Elements.Num() * 1.5f);

		const double SplitValue = SplitCenter[MaxAxis];
		for (const FElement& Element : NodeToProcess->Elements)
		{
			const FVector2D ElementCenter = Element.Bounds.GetCenter();
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
			TVoxelSet<FVoxelBox2D> Elements0;
			TVoxelSet<FVoxelBox2D> Elements1;
			for (const FElement& Element : ChildToProcess0->Elements)
			{
				Elements0.Add(Element.Bounds);
			}
			for (const FElement& Element : ChildToProcess1->Elements)
			{
				Elements1.Add(Element.Bounds);
			}
			ensure(
				Elements0.Num() != ChildToProcess0->Elements.Num() ||
				Elements1.Num() != ChildToProcess1->Elements.Num());
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

void FVoxelAABBTree2D::Shrink()
{
	VOXEL_FUNCTION_COUNTER();

	Nodes.Shrink();
	Leaves.Shrink();
}

TSharedRef<FVoxelAABBTree2D> FVoxelAABBTree2D::Create(TVoxelArray<FElement>&& Elements)
{
	const TSharedRef<FVoxelAABBTree2D> Tree = MakeShared<FVoxelAABBTree2D>();
	Tree->Initialize(MoveTemp(Elements));
	return Tree;
}

TSharedRef<FVoxelAABBTree2D> FVoxelAABBTree2D::Create(const TConstVoxelArrayView<FVoxelBox2D> Bounds)
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

	return Create(MoveTemp(Elements));
}

TSharedRef<FVoxelAABBTree2D> FVoxelAABBTree2D::Create(const TVoxelChunkedArray<FVoxelBox2D>& Bounds)
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

	return Create(MoveTemp(Elements));
}

TSharedRef<FVoxelAABBTree2D> FVoxelAABBTree2D::Create(const TVoxelChunkedArray<FVoxelIntBox2D>& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<FElement> Elements;
	FVoxelUtilities::SetNumFast(Elements, Bounds.Num());

	for (int32 Index = 0; Index < Bounds.Num(); Index++)
	{
		Elements[Index] = FElement
		{
			FVoxelBox2D(Bounds[Index].Min, Bounds[Index].Max),
			Index
		};
	}

	return Create(MoveTemp(Elements));
}