// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelFastAABBTree
{
public:
	struct FElementArray
	{
		TVoxelArray<int32> Payload;
		TVoxelArray<float> MinX;
		TVoxelArray<float> MinY;
		TVoxelArray<float> MinZ;
		TVoxelArray<float> MaxX;
		TVoxelArray<float> MaxY;
		TVoxelArray<float> MaxZ;

		FORCEINLINE int32 Num() const
		{
			checkVoxelSlow(Payload.Num() == MinX.Num());
			checkVoxelSlow(Payload.Num() == MinY.Num());
			checkVoxelSlow(Payload.Num() == MinZ.Num());
			checkVoxelSlow(Payload.Num() == MaxX.Num());
			checkVoxelSlow(Payload.Num() == MaxY.Num());
			checkVoxelSlow(Payload.Num() == MaxZ.Num());
			return Payload.Num();
		}
		FORCEINLINE void SetNum(const int32 Number)
		{
			FVoxelUtilities::SetNumFast(Payload, Number);
			FVoxelUtilities::SetNumFast(MinX, Number);
			FVoxelUtilities::SetNumFast(MinY, Number);
			FVoxelUtilities::SetNumFast(MinZ, Number);
			FVoxelUtilities::SetNumFast(MaxX, Number);
			FVoxelUtilities::SetNumFast(MaxY, Number);
			FVoxelUtilities::SetNumFast(MaxZ, Number);
		}
	};

	struct FElementArrayView
	{
		TVoxelArrayView<int32> Payload;
		TVoxelArrayView<float> MinX;
		TVoxelArrayView<float> MinY;
		TVoxelArrayView<float> MinZ;
		TVoxelArrayView<float> MaxX;
		TVoxelArrayView<float> MaxY;
		TVoxelArrayView<float> MaxZ;

		FORCEINLINE int32 Num() const
		{
			checkVoxelSlow(Payload.Num() == MinX.Num());
			checkVoxelSlow(Payload.Num() == MinY.Num());
			checkVoxelSlow(Payload.Num() == MinZ.Num());
			checkVoxelSlow(Payload.Num() == MaxX.Num());
			checkVoxelSlow(Payload.Num() == MaxY.Num());
			checkVoxelSlow(Payload.Num() == MaxZ.Num());
			return Payload.Num();
		}
		FORCEINLINE void Swap(const int32 IndexA, const int32 IndexB)
		{
			checkVoxelSlow(IndexA != IndexB);

			::Swap(Payload[IndexA], Payload[IndexB]);
			::Swap(MinX[IndexA], MinX[IndexB]);
			::Swap(MinY[IndexA], MinY[IndexB]);
			::Swap(MinZ[IndexA], MinZ[IndexB]);
			::Swap(MaxX[IndexA], MaxX[IndexB]);
			::Swap(MaxY[IndexA], MaxY[IndexB]);
			::Swap(MaxZ[IndexA], MaxZ[IndexB]);
		}

		FElementArray Array() const
		{
			FElementArray Result;
			Result.Payload = TVoxelArray<int32>(Payload);
			Result.MinX = TVoxelArray<float>(MinX);
			Result.MinY = TVoxelArray<float>(MinY);
			Result.MinZ = TVoxelArray<float>(MinZ);
			Result.MaxX = TVoxelArray<float>(MaxX);
			Result.MaxY = TVoxelArray<float>(MaxY);
			Result.MaxZ = TVoxelArray<float>(MaxZ);
			return Result;
		}
	};

	struct FNode
	{
		FVector3f ChildBounds0_Min{ ForceInit };
		FVector3f ChildBounds0_Max{ ForceInit };
		FVector3f ChildBounds1_Min{ ForceInit };
		FVector3f ChildBounds1_Max{ ForceInit };

		union
		{
			struct
			{
				int32 ChildIndex0;
				int32 ChildIndex1;
			};
			int32 LeafIndex;
		};

		bool bLeaf = false;

		FNode()
		{
			ChildIndex0 = -1;
			ChildIndex1 = -1;
		}
	};
	struct FLeaf
	{
		FElementArrayView Elements;
	};

	const int32 MaxChildrenInLeaf;
	const int32 MaxTreeDepth;

	explicit FVoxelFastAABBTree(
		const int32 MaxChildrenInLeaf = 12,
		const int32 MaxTreeDepth = 16)
		: MaxChildrenInLeaf(MaxChildrenInLeaf)
		, MaxTreeDepth(MaxTreeDepth)
	{
	}

	void Initialize(FElementArray&& Elements);
	void Shrink();

public:
	FORCEINLINE TConstVoxelArrayView<FNode> GetNodes() const
	{
		return Nodes;
	}
	FORCEINLINE TConstVoxelArrayView<FLeaf> GetLeaves() const
	{
		return Leaves;
	}

public:
	bool Intersects(
		const FVector3f& BoundsMin,
		const FVector3f& BoundsMax) const
	{
		return Intersects(BoundsMin, BoundsMax, [](int32)
		{
			return true;
		});
	}
	template<typename LambdaType>
	bool Intersects(
		const FVector3f& BoundsMin,
		const FVector3f& BoundsMax,
		LambdaType&& CustomCheck) const
	{
		if (Nodes.Num() == 0)
		{
			return false;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add_EnsureNoGrow(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];
				for (int32 Index = 0; Index < Leaf.Elements.Num(); Index++)
				{
					if (!Intersects(
						BoundsMin,
						BoundsMax,
						FVector3f(
							Leaf.Elements.MinX[Index],
							Leaf.Elements.MinY[Index],
							Leaf.Elements.MinZ[Index]),
						FVector3f(
							Leaf.Elements.MaxX[Index],
							Leaf.Elements.MaxY[Index],
							Leaf.Elements.MaxZ[Index])))
					{
						continue;
					}

					if (CustomCheck(Leaf.Elements.Payload[Index]))
					{
						return true;
					}
				}
			}
			else
			{
				if (Intersects(
					BoundsMin,
					BoundsMax,
					Node.ChildBounds0_Min,
					Node.ChildBounds0_Max))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex0);
				}

				if (Intersects(
					BoundsMin,
					BoundsMax,
					Node.ChildBounds1_Min,
					Node.ChildBounds1_Max))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex1);
				}
			}
		}

		return false;
	}

public:
	template<typename ShouldVisitType, typename VisitType>
	void Traverse(ShouldVisitType&& ShouldVisit, VisitType&& Visit) const
	{
		if (Nodes.Num() == 0)
		{
			return;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add_EnsureNoGrow(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];

				for (int32 Index = 0; Index < Leaf.Elements.Num(); Index++)
				{
					if (!ShouldVisit(
						FVector3f(
							Leaf.Elements.MinX[Index],
							Leaf.Elements.MinY[Index],
							Leaf.Elements.MinZ[Index]),
						FVector3f(
							Leaf.Elements.MaxX[Index],
							Leaf.Elements.MaxY[Index],
							Leaf.Elements.MaxZ[Index])))
					{
						continue;
					}

					Visit(Leaf.Elements.Payload[Index]);
				}
			}
			else
			{
				if (ShouldVisit(Node.ChildBounds0_Min, Node.ChildBounds0_Max))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex0);
				}
				if (ShouldVisit(Node.ChildBounds1_Min, Node.ChildBounds1_Max))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex1);
				}
			}
		}
	}
	template<typename VisitType>
	void Traverse(
		const FVector3f& BoundsMin,
		const FVector3f& BoundsMax,
		VisitType&& Visit) const
	{
		this->Traverse(
			[&](const FVector3f& Min, const FVector3f& Max)
			{
				return
					Min.X <= BoundsMax.X &&
					Min.Y <= BoundsMax.Y &&
					Min.Z <= BoundsMax.Z &&
					BoundsMin.X <= Max.X &&
					BoundsMin.Y <= Max.Y &&
					BoundsMin.Z <= Max.Z;
			},
			MoveTemp(Visit));
	}

private:
	TVoxelArray<FNode> Nodes;
	TVoxelArray<FLeaf> Leaves;
	FElementArray Elements;

	FORCEINLINE static bool Intersects(
		const FVector3f& MinA,
		const FVector3f& MaxA,
		const FVector3f& MinB,
		const FVector3f& MaxB)
	{
		if (MinA.X > MaxB.X || MinB.X > MaxA.X)
		{
			return false;
		}

		if (MinA.Y > MaxB.Y || MinB.Y > MaxA.Y)
		{
			return false;
		}

		if (MinA.Z > MaxB.Z || MinB.Z > MaxA.Z)
		{
			return false;
		}

		return true;
	}
};