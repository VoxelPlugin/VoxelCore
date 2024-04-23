// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelFastAABBTree.h"
#include "VoxelWelfordVariance.h"
#include "VoxelFastAABBTreeImpl.ispc.generated.h"

void FVoxelFastAABBTree::Initialize(FElementArray&& InElements)
{
	Elements = MoveTemp(InElements);

	VOXEL_FUNCTION_COUNTER_NUM(Elements.Num(), 128);
	check(Nodes.Num() == 0);
	check(Leaves.Num() == 0);

	const int32 NumElements = Elements.Num();
	const int32 ExpectedNumLeaves = 2 * FVoxelUtilities::DivideCeil(NumElements, MaxChildrenInLeaf);
	const int32 ExpectedNumNodes = 2 * ExpectedNumLeaves;

	Nodes.Reserve(ExpectedNumNodes);
	Leaves.Reserve(ExpectedNumLeaves);

	struct FNodeToProcess
	{
		FElementArrayView Elements;

		int32 NodeLevel = -1;
		int32 NodeIndex = -1;

		float MinX = 0.f;
		float MinY = 0.f;
		float MinZ = 0.f;
		float MaxX = 0.f;
		float MaxY = 0.f;
		float MaxZ = 0.f;

		float AverageX = 0.f;
		float AverageY = 0.f;
		float AverageZ = 0.f;

		float VarianceX = 0.f;
		float VarianceY = 0.f;
		float VarianceZ = 0.f;

		void Compute()
		{
			ispc::VoxelFastAABBTree_Compute(
				Elements.MinX.GetData(),
				Elements.MinY.GetData(),
				Elements.MinZ.GetData(),
				Elements.MaxX.GetData(),
				Elements.MaxY.GetData(),
				Elements.MaxZ.GetData(),
				Elements.Num(),
				MinX,
				MinY,
				MinZ,
				MaxX,
				MaxY,
				MaxZ,
				AverageX,
				AverageY,
				AverageZ,
				VarianceX,
				VarianceY,
				VarianceZ);
		}
	};

	TVoxelChunkedArray<FNodeToProcess> NodesToProcess;

	// Create root node
	{
		FNodeToProcess& RootNode = NodesToProcess.Emplace_GetRef();
		RootNode.Elements.Payload = Elements.Payload;
		RootNode.Elements.MinX = Elements.MinX;
		RootNode.Elements.MinY = Elements.MinY;
		RootNode.Elements.MinZ = Elements.MinZ;
		RootNode.Elements.MaxX = Elements.MaxX;
		RootNode.Elements.MaxY = Elements.MaxY;
		RootNode.Elements.MaxZ = Elements.MaxZ;
		RootNode.NodeLevel = 0;
		RootNode.NodeIndex = Nodes.Emplace();
		RootNode.Compute();
	}

	while (NodesToProcess.Num())
	{
		FNodeToProcess Parent = NodesToProcess.Pop();

		Nodes.Reserve(Nodes.Num() + 2);

		// Check Node will not be invalidated
		const int32 CurrentNodesMax = Nodes.Max();
		ON_SCOPE_EXIT
		{
			checkVoxelSlow(CurrentNodesMax == Nodes.Max());
		};

		FNode& ParentNode = Nodes[Parent.NodeIndex];

		if (Parent.Elements.Num() <= MaxChildrenInLeaf ||
			Parent.NodeLevel >= MaxTreeDepth)
		{
			ParentNode.bLeaf = true;
			ParentNode.LeafIndex = Leaves.Add(FLeaf{ Parent.Elements });
			continue;
		}

		FNodeToProcess& Child0 = NodesToProcess.Emplace_GetRef();
		FNodeToProcess& Child1 = NodesToProcess.Emplace_GetRef();

		Child0.NodeIndex = Nodes.Emplace();
		Child1.NodeIndex = Nodes.Emplace();

		const EVoxelAxis SplitAxis = INLINE_LAMBDA
		{
			if (Parent.VarianceX > Parent.VarianceY &&
				Parent.VarianceX > Parent.VarianceZ)
			{
				return EVoxelAxis::X;
			}
			else if (Parent.VarianceY > Parent.VarianceZ)
			{
				return EVoxelAxis::Y;
			}
			else
			{
				return EVoxelAxis::Z;
			}
		};

		const float SplitValue = INLINE_LAMBDA
		{
			switch (SplitAxis)
			{
			default: VOXEL_ASSUME(false);
			case EVoxelAxis::X: return Parent.AverageX;
			case EVoxelAxis::Y: return Parent.AverageY;
			case EVoxelAxis::Z: return Parent.AverageZ;
			}
		};

		const TConstVoxelArrayView<float> Min = INLINE_LAMBDA -> TConstVoxelArrayView<float>
		{
			switch (SplitAxis)
			{
			default: VOXEL_ASSUME(false);
			case EVoxelAxis::X: return Parent.Elements.MinX;
			case EVoxelAxis::Y: return Parent.Elements.MinY;
			case EVoxelAxis::Z: return Parent.Elements.MinZ;
			}
		};

		const TConstVoxelArrayView<float> Max = INLINE_LAMBDA -> TConstVoxelArrayView<float>
		{
			switch (SplitAxis)
			{
			default: VOXEL_ASSUME(false);
			case EVoxelAxis::X: return Parent.Elements.MaxX;
			case EVoxelAxis::Y: return Parent.Elements.MaxY;
			case EVoxelAxis::Z: return Parent.Elements.MaxZ;
			}
		};

		{
			const float SplitValueTimes2 = SplitValue * 2.f;

			int32 Index0 = 0;
			int32 Index1 = Parent.Elements.Num() - 1;

			const auto Is0 = [&](const int32 Index)
			{
				// return (Min[Index] + Max[Index]) / 2.f <= SplitValue;
				return Min[Index] + Max[Index] <= SplitValueTimes2;
			};

			while (Index0 < Index1)
			{
				if (Is0(Index0))
				{
					Index0++;
					continue;
				}
				if (!Is0(Index1))
				{
					Index1--;
					continue;
				}

				checkVoxelSlow(!Is0(Index0));
				checkVoxelSlow(Is0(Index1));

				Parent.Elements.Swap(Index0, Index1);

				checkVoxelSlow(Is0(Index0));
				checkVoxelSlow(!Is0(Index1));

				Index0++;
				Index1--;
			}

			const int32 Num0 = Is0(Index0) ? Index0 + 1 : Index0;

			if (VOXEL_DEBUG)
			{
				for (int32 Index = 0; Index < Num0; Index++)
				{
					check(Is0(Index));
				}
				for (int32 Index = Num0; Index < Parent.Elements.Num(); Index++)
				{
					check(!Is0(Index));
				}
			}

			Child0.Elements.Payload = Parent.Elements.Payload.LeftOf(Num0);
			Child0.Elements.MinX = Parent.Elements.MinX.LeftOf(Num0);
			Child0.Elements.MinY = Parent.Elements.MinY.LeftOf(Num0);
			Child0.Elements.MinZ = Parent.Elements.MinZ.LeftOf(Num0);
			Child0.Elements.MaxX = Parent.Elements.MaxX.LeftOf(Num0);
			Child0.Elements.MaxY = Parent.Elements.MaxY.LeftOf(Num0);
			Child0.Elements.MaxZ = Parent.Elements.MaxZ.LeftOf(Num0);

			Child1.Elements.Payload = Parent.Elements.Payload.RightOf(Num0);
			Child1.Elements.MinX = Parent.Elements.MinX.RightOf(Num0);
			Child1.Elements.MinY = Parent.Elements.MinY.RightOf(Num0);
			Child1.Elements.MinZ = Parent.Elements.MinZ.RightOf(Num0);
			Child1.Elements.MaxX = Parent.Elements.MaxX.RightOf(Num0);
			Child1.Elements.MaxY = Parent.Elements.MaxY.RightOf(Num0);
			Child1.Elements.MaxZ = Parent.Elements.MaxZ.RightOf(Num0);
		}

		// Failed to split
		if (Child0.Elements.Num() == 0 ||
			Child1.Elements.Num() == 0)
		{
			ensure(false);
			ParentNode.bLeaf = true;
			ParentNode.LeafIndex = Leaves.Add(FLeaf{ Parent.Elements });
			continue;
		}

		Child0.Compute();
		Child1.Compute();

		ParentNode.bLeaf = false;

		ParentNode.ChildBounds0_Min = FVector3f(Child0.MinX, Child0.MinY, Child0.MinZ);
		ParentNode.ChildBounds0_Max = FVector3f(Child0.MaxX, Child0.MaxY, Child0.MaxZ);

		ParentNode.ChildBounds1_Min = FVector3f(Child1.MinX, Child1.MinY, Child1.MinZ);
		ParentNode.ChildBounds1_Max = FVector3f(Child1.MaxX, Child1.MaxY, Child1.MaxZ);

		ParentNode.ChildIndex0 = Child0.NodeIndex;
		ParentNode.ChildIndex1 = Child1.NodeIndex;
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

void FVoxelFastAABBTree::Shrink()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_ALLOW_REALLOC_SCOPE();

	Nodes.Shrink();
	Leaves.Shrink();
}