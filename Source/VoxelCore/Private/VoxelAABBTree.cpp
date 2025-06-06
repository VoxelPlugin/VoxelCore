// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelAABBTree.h"
#include "VoxelWelfordVariance.h"
#include "VoxelAABBTreeImpl.ispc.generated.h"

#if 0
VOXEL_RUN_ON_STARTUP_GAME()
{
	FVoxelAABBTree::FElementArray Elements;
	Elements.SetNum(100 * 100 * 5);

	for (int32 X = 0; X < 100; X++)
	{
		for (int32 Y = 0; Y < 100; Y++)
		{
			for (int32 Z = 0; Z < 5; Z++)
			{
				Elements.Payload[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z);

				Elements.MinX[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * X;
				Elements.MinY[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * Y;
				Elements.MinZ[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * Z;

				Elements.MaxX[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * X + 10;
				Elements.MaxY[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * Y + 10;
				Elements.MaxZ[FVoxelUtilities::Get3DIndex<int32>(100, X, Y, Z)] = 10 * Z + 10;
			}
		}
	}

	const TSharedRef<FVoxelAABBTree> Tree = FVoxelAABBTree::Create(MoveTemp(Elements));

	while (true)
	{
		VOXEL_LOG_FUNCTION_STATS();

		int64 Result = 0;
		for (int32 X = 0; X < 100; X++)
		{
			for (int32 Y = 0; Y < 100; Y++)
			{
				for (int32 Z = 0; Z < 5; Z++)
				{
					Tree->TraverseBounds(
						FVector3f(10 * X, 10 * Y, 10 * Z),
						FVector3f(10 * X, 10 * Y, 10 * Z) + 10,
						[&](const int32 Index)
						{
							Result += Index;
						});
				}
			}
		}
		check(Result == 28860722774);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelAABBTree::FVoxelAABBTree(
	const int32 MaxChildrenInLeaf,
	const int32 MaxTreeDepth)
	: MaxChildrenInLeaf(MaxChildrenInLeaf)
	, MaxTreeDepth(MaxTreeDepth)
{
	check(MaxChildrenInLeaf >= 1);
}

void FVoxelAABBTree::Initialize(FElementArray&& Elements)
{
	if (Elements.Num() == 0)
	{
		return;
	}

#if VOXEL_DEBUG && 0
	const FElementArray ElementsCopy = Elements;

	ON_SCOPE_EXIT
	{
		TVoxelMap<int32, int32> PayloadToIndex;
		PayloadToIndex.Reserve(ElementsCopy.Num());

		for (int32 Index = 0; Index < ElementsCopy.Num(); Index++)
		{
			PayloadToIndex.Add_EnsureNew(ElementsCopy.Payload[Index], Index);
		}

		for (int32 Index = 0; Index < ElementsCopy.Num(); Index++)
		{
			const int32 OldIndex = PayloadToIndex[Payloads[Index]];

			check(ElementsCopy.MinX[OldIndex] == ElementBounds[Index].GetMin().X);
			check(ElementsCopy.MinY[OldIndex] == ElementBounds[Index].GetMin().Y);
			check(ElementsCopy.MinZ[OldIndex] == ElementBounds[Index].GetMin().Z);

			check(ElementsCopy.MaxX[OldIndex] == ElementBounds[Index].GetMax().X);
			check(ElementsCopy.MaxY[OldIndex] == ElementBounds[Index].GetMax().Y);
			check(ElementsCopy.MaxZ[OldIndex] == ElementBounds[Index].GetMax().Z);
		}
	};
#endif

	VOXEL_FUNCTION_COUNTER_NUM(Elements.Num());
	check(Nodes.Num() == 0);
	check(Leaves.Num() == 0);

	// Ensure we have enough padding
	Elements.Reserve(Elements.Num());

	const int32 NumElements = Elements.Num();
	const int32 ExpectedNumLeaves = 2 * FVoxelUtilities::DivideCeil(NumElements, MaxChildrenInLeaf);
	const int32 ExpectedNumNodes = 2 * ExpectedNumLeaves;

	Nodes.Reserve(ExpectedNumNodes);
	Leaves.Reserve(ExpectedNumLeaves);

	struct FNodeToProcess
	{
		int32 StartIndex = -1;
		int32 EndIndex = -1;

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

		FORCEINLINE int32 Num() const
		{
			return EndIndex - StartIndex;
		}

		void ComputeVariance(const FElementArray& Elements)
		{
			checkVoxelSlow(Num() >= 1);

			if (Num() == 1)
			{
				MinX = Elements.MinX[StartIndex];
				MinY = Elements.MinY[StartIndex];
				MinZ = Elements.MinZ[StartIndex];
				MaxX = Elements.MaxX[StartIndex];
				MaxY = Elements.MaxY[StartIndex];
				MaxZ = Elements.MaxZ[StartIndex];

				AverageX = (Elements.MinX[StartIndex] + Elements.MaxX[StartIndex]) / 2.f;
				AverageY = (Elements.MinY[StartIndex] + Elements.MaxY[StartIndex]) / 2.f;
				AverageZ = (Elements.MinZ[StartIndex] + Elements.MaxZ[StartIndex]) / 2.f;

				VarianceX = 0;
				VarianceY = 0;
				VarianceZ = 0;

				return;
			}

			if (Num() > 8)
			{
				ispc::VoxelAABBTree_ComputeVariance(
					Elements.MinX.GetData() + StartIndex,
					Elements.MinY.GetData() + StartIndex,
					Elements.MinZ.GetData() + StartIndex,
					Elements.MaxX.GetData() + StartIndex,
					Elements.MaxY.GetData() + StartIndex,
					Elements.MaxZ.GetData() + StartIndex,
					Num(),
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

				return;
			}

			float ScaledVarianceX = 0.f;
			float ScaledVarianceY = 0.f;
			float ScaledVarianceZ = 0.f;

			MinX = MAX_flt;
			MinY = MAX_flt;
			MinZ = MAX_flt;
			MaxX = -MAX_flt;
			MaxY = -MAX_flt;
			MaxZ = -MAX_flt;

			for (int32 Index = StartIndex; Index < EndIndex; Index++)
			{
				MinX = FMath::Min(MinX, Elements.MinX[Index]);
				MinY = FMath::Min(MinY, Elements.MinY[Index]);
				MinZ = FMath::Min(MinZ, Elements.MinZ[Index]);
				MaxX = FMath::Max(MaxX, Elements.MaxX[Index]);
				MaxY = FMath::Max(MaxY, Elements.MaxY[Index]);
				MaxZ = FMath::Max(MaxZ, Elements.MaxZ[Index]);

				const float X = (Elements.MinX[Index] + Elements.MaxX[Index]) / 2.f;
				const float Y = (Elements.MinY[Index] + Elements.MaxY[Index]) / 2.f;
				const float Z = (Elements.MinZ[Index] + Elements.MaxZ[Index]) / 2.f;
				const float NumAdded = (Index - StartIndex) + 1;

				{
					const float Delta = X - AverageX;
					AverageX += Delta / NumAdded;
					ScaledVarianceX += Delta * (X - AverageX);
				}
				{
					const float Delta = Y - AverageY;
					AverageY += Delta / NumAdded;
					ScaledVarianceY += Delta * (Y - AverageY);
				}
				{
					const float Delta = Z - AverageZ;
					AverageZ += Delta / NumAdded;
					ScaledVarianceZ += Delta * (Z - AverageZ);
				}
			}

			VarianceX = ScaledVarianceX / (Num() - 1);
			VarianceY = ScaledVarianceY / (Num() - 1);
			VarianceZ = ScaledVarianceZ / (Num() - 1);
		}
	};

	TVoxelChunkedArray<FNodeToProcess> NodesToProcess;

	// Create root node
	{
		FNodeToProcess& RootNode = NodesToProcess.Emplace_GetRef();
		RootNode.StartIndex = 0;
		RootNode.EndIndex = Elements.Num();
		RootNode.NodeLevel = 0;
		RootNode.NodeIndex = Nodes.Emplace();
		RootNode.ComputeVariance(Elements);

		RootBounds = FVoxelFastBox(
			FVector3f(RootNode.MinX, RootNode.MinY, RootNode.MinZ),
			FVector3f(RootNode.MaxX, RootNode.MaxY, RootNode.MaxZ));
	}

	// TODO perf counter per depth
	while (NodesToProcess.Num())
	{
		FNodeToProcess Parent = NodesToProcess.Pop();

		Nodes.ReserveGrow(2);

		// Check Node will not be invalidated
		const int32 CurrentNodesMax = Nodes.Max();
		ON_SCOPE_EXIT
		{
			checkVoxelSlow(CurrentNodesMax == Nodes.Max());
		};

		FNode& ParentNode = Nodes[Parent.NodeIndex];

		if (Parent.Num() <= MaxChildrenInLeaf ||
			Parent.NodeLevel >= MaxTreeDepth)
		{
			ParentNode.bLeaf = true;
			ParentNode.LeafIndex = Leaves.Add(FLeaf
			{
				Parent.StartIndex,
				Parent.EndIndex
			});
			continue;
		}

		FNodeToProcess& Child0 = NodesToProcess.Emplace_GetRef();
		FNodeToProcess& Child1 = NodesToProcess.Emplace_GetRef();

		Child0.NodeIndex = Nodes.Emplace_EnsureNoGrow();
		Child1.NodeIndex = Nodes.Emplace_EnsureNoGrow();

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
			case EVoxelAxis::X: return Elements.MinX;
			case EVoxelAxis::Y: return Elements.MinY;
			case EVoxelAxis::Z: return Elements.MinZ;
			}
		};

		const TConstVoxelArrayView<float> Max = INLINE_LAMBDA -> TConstVoxelArrayView<float>
		{
			switch (SplitAxis)
			{
			default: VOXEL_ASSUME(false);
			case EVoxelAxis::X: return Elements.MaxX;
			case EVoxelAxis::Y: return Elements.MaxY;
			case EVoxelAxis::Z: return Elements.MaxZ;
			}
		};

		{
			const float SplitValueTimes2 = SplitValue * 2.f;

			const auto Is0 = [&](const int32 Index)
			{
				// return (Min[Index] + Max[Index]) / 2.f <= SplitValue;
				return Min[Index] + Max[Index] <= SplitValueTimes2;
			};

			int32 SplitIndex;
			if (Parent.Num() < 16)
			{
				int32 Index0 = Parent.StartIndex;
				int32 Index1 = Parent.EndIndex - 1;

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

					checkVoxelSlow(Index0 != Index1);

					Swap(Elements.Payload[Index0], Elements.Payload[Index1]);
					Swap(Elements.MinX[Index0], Elements.MinX[Index1]);
					Swap(Elements.MinY[Index0], Elements.MinY[Index1]);
					Swap(Elements.MinZ[Index0], Elements.MinZ[Index1]);
					Swap(Elements.MaxX[Index0], Elements.MaxX[Index1]);
					Swap(Elements.MaxY[Index0], Elements.MaxY[Index1]);
					Swap(Elements.MaxZ[Index0], Elements.MaxZ[Index1]);

					checkVoxelSlow(Is0(Index0));
					checkVoxelSlow(!Is0(Index1));

					Index0++;
					Index1--;
				}

				SplitIndex = Is0(Index0) ? Index0 + 1 : Index0;
			}
			else
			{
				SplitIndex = INLINE_LAMBDA
				{
					switch (SplitAxis)
					{
					default: VOXEL_ASSUME(false);
					case EVoxelAxis::X:
					{
						return ispc::VoxelAABBTree_Split_X(
							Elements.Payload.GetData(),
							Elements.MinX.GetData(),
							Elements.MinY.GetData(),
							Elements.MinZ.GetData(),
							Elements.MaxX.GetData(),
							Elements.MaxY.GetData(),
							Elements.MaxZ.GetData(),
							SplitValue,
							Parent.StartIndex,
							Parent.EndIndex,
							Elements.Max());
					}
					case EVoxelAxis::Y:
					{
						return ispc::VoxelAABBTree_Split_X(
							Elements.Payload.GetData(),
							Elements.MinY.GetData(),
							Elements.MinZ.GetData(),
							Elements.MinX.GetData(),
							Elements.MaxY.GetData(),
							Elements.MaxZ.GetData(),
							Elements.MaxX.GetData(),
							SplitValue,
							Parent.StartIndex,
							Parent.EndIndex,
							Elements.Max());
					}
					case EVoxelAxis::Z:
					{
						return ispc::VoxelAABBTree_Split_X(
							Elements.Payload.GetData(),
							Elements.MinZ.GetData(),
							Elements.MinX.GetData(),
							Elements.MinY.GetData(),
							Elements.MaxZ.GetData(),
							Elements.MaxX.GetData(),
							Elements.MaxY.GetData(),
							SplitValue,
							Parent.StartIndex,
							Parent.EndIndex,
							Elements.Max());
					}
					}
				};
			}

			if (VOXEL_DEBUG)
			{
				for (int32 Index = Parent.StartIndex; Index < SplitIndex; Index++)
				{
					check(Is0(Index));
				}
				for (int32 Index = SplitIndex; Index < Parent.EndIndex; Index++)
				{
					check(!Is0(Index));
				}
			}

			Child0.StartIndex = Parent.StartIndex;
			Child0.EndIndex = SplitIndex;

			Child1.StartIndex = SplitIndex;
			Child1.EndIndex = Parent.EndIndex;
		}

		// Failed to split
		if (Child0.Num() == 0 ||
			Child1.Num() == 0)
		{
#if VOXEL_DEBUG
			TVoxelSet<FVoxelBox> Elements0;
			TVoxelSet<FVoxelBox> Elements1;
			for (int32 Index = Child0.StartIndex; Index < Child0.EndIndex; Index++)
			{
				Elements0.Add(FVoxelBox(
					FVector3f(Elements.MinX[Index], Elements.MinY[Index], Elements.MinZ[Index]),
					FVector3f(Elements.MaxX[Index], Elements.MaxY[Index], Elements.MaxZ[Index])));
			}
			for (int32 Index = Child1.StartIndex; Index < Child1.EndIndex; Index++)
			{
				Elements1.Add(FVoxelBox(
					FVector3f(Elements.MinX[Index], Elements.MinY[Index], Elements.MinZ[Index]),
					FVector3f(Elements.MaxX[Index], Elements.MaxY[Index], Elements.MaxZ[Index])));
			}
			ensure(
				Elements0.Num() != Child0.Num() ||
				Elements1.Num() != Child1.Num());
#endif

			ParentNode.bLeaf = true;
			ParentNode.LeafIndex = Leaves.Add(FLeaf
			{
				Parent.StartIndex,
				Parent.EndIndex
			});

			NodesToProcess.Pop();
			NodesToProcess.Pop();
			continue;
		}

		Child0.ComputeVariance(Elements);
		Child1.ComputeVariance(Elements);

		ParentNode.bLeaf = false;

		ParentNode.ChildBounds0 = FVoxelFastBox(
			FVector3f(Child0.MinX, Child0.MinY, Child0.MinZ),
			FVector3f(Child0.MaxX, Child0.MaxY, Child0.MaxZ));

		ParentNode.ChildBounds1 = FVoxelFastBox(
			FVector3f(Child1.MinX, Child1.MinY, Child1.MinZ),
			FVector3f(Child1.MaxX, Child1.MaxY, Child1.MaxZ));

		ParentNode.ChildIndex0 = Child0.NodeIndex;
		ParentNode.ChildIndex1 = Child1.NodeIndex;
	}

	{
		VOXEL_SCOPE_COUNTER("WriteElementBounds");

		FVoxelUtilities::SetNumFast(ElementBounds, Elements.Num());

		for (int32 Index = 0; Index < ElementBounds.Num(); Index++)
		{
			ElementBounds[Index] = FVoxelFastBox(
				FVector3f(Elements.MinX[Index], Elements.MinY[Index], Elements.MinZ[Index]),
				FVector3f(Elements.MaxX[Index], Elements.MaxY[Index], Elements.MaxZ[Index]));
		}
	}

	Payloads = MoveTemp(Elements.Payload);

#if VOXEL_DEBUG
	int32 NumElementsInLeaves = 0;
	for (const FLeaf& Leaf : Leaves)
	{
		NumElementsInLeaves += Leaf.Num();
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

void FVoxelAABBTree::DrawTree(
	const TVoxelObjectPtr<UWorld> World,
	const FLinearColor& Color,
	const FTransform& Transform,
	int32 Index) const
{
	VOXEL_FUNCTION_COUNTER();

	if (IsEmpty())
	{
		return;
	}

	int32 MaxDepth = 0;
	const TFunction<void(const FNode&, int32)> FindMaxDepth = [&](const FNode& Node, const int32 Depth)
	{
		MaxDepth = FMath::Max(MaxDepth, Depth);

		if (Node.bLeaf)
		{
			return;
		}

		FindMaxDepth(Nodes[Node.ChildIndex0], Depth + 1);
		FindMaxDepth(Nodes[Node.ChildIndex1], Depth + 1);
	};
	FindMaxDepth(Nodes[0], 0);

	if (!ensureVoxelSlow(MaxDepth != 0))
	{
		return;
	}

	Index = Index % MaxDepth;

	const TFunction<void(const FNode&, int32)> Iterate = [&](const FNode& Node, const int32 Depth)
	{
		if (Node.bLeaf)
		{
			return;
		}

		if (Depth == Index)
		{
			FVoxelDebugDrawer(World).Color(Color).OneFrame().DrawBox(Node.ChildBounds0.GetBox(), Transform);
			FVoxelDebugDrawer(World).Color(Color).OneFrame().DrawBox(Node.ChildBounds1.GetBox(), Transform);
			return;
		}

		Iterate(Nodes[Node.ChildIndex0], Depth + 1);
		Iterate(Nodes[Node.ChildIndex1], Depth + 1);
	};
	Iterate(Nodes[0], 0);
}

TSharedRef<FVoxelAABBTree> FVoxelAABBTree::Create(FElementArray&& Elements)
{
	const TSharedRef<FVoxelAABBTree> Tree = MakeShared<FVoxelAABBTree>();
	Tree->Initialize(MoveTemp(Elements));
	return Tree;
}

TSharedRef<FVoxelAABBTree> FVoxelAABBTree::Create(const TConstVoxelArrayView<FVoxelBox> Bounds)
{
	VOXEL_FUNCTION_COUNTER_NUM(Bounds.Num());

	FElementArray Elements;
	Elements.SetNum(Bounds.Num());

	for (int32 Index = 0; Index < Bounds.Num(); Index++)
	{
		const FVoxelBox& Box = Bounds[Index];

		Elements.MinX[Index] = FVoxelUtilities::DoubleToFloat_Lower(Box.Min.X);
		Elements.MinY[Index] = FVoxelUtilities::DoubleToFloat_Lower(Box.Min.Y);
		Elements.MinZ[Index] = FVoxelUtilities::DoubleToFloat_Lower(Box.Min.Z);
		Elements.MaxX[Index] = FVoxelUtilities::DoubleToFloat_Higher(Box.Max.X);
		Elements.MaxY[Index] = FVoxelUtilities::DoubleToFloat_Higher(Box.Max.Y);
		Elements.MaxZ[Index] = FVoxelUtilities::DoubleToFloat_Higher(Box.Max.Z);
		Elements.Payload[Index] = Index;
	}

	return Create(MoveTemp(Elements));
}