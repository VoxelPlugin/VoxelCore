// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelAABBTree
{
public:
	struct FElement
	{
		FVoxelBox Bounds;
		int32 Payload = -1;
	};
	struct FNode
	{
		FVoxelBox ChildBounds0;
		FVoxelBox ChildBounds1;

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
		TVoxelArray<FElement> Elements;
	};

	const int32 MaxChildrenInLeaf;
	const int32 MaxTreeDepth;

	explicit FVoxelAABBTree(
		const int32 MaxChildrenInLeaf = 12,
		const int32 MaxTreeDepth = 16)
		: MaxChildrenInLeaf(MaxChildrenInLeaf)
		, MaxTreeDepth(MaxTreeDepth)
	{
	}

	void Initialize(TVoxelArray<FElement>&& Elements);
	void Shrink();

public:
	FORCEINLINE const FVoxelBox& GetBounds() const
	{
		ensure(RootBounds.IsValidAndNotEmpty());
		return RootBounds;
	}
	FORCEINLINE TConstVoxelArrayView<FNode> GetNodes() const
	{
		return Nodes;
	}
	FORCEINLINE TConstVoxelArrayView<FLeaf> GetLeaves() const
	{
		return Leaves;
	}

public:
	using FBulkRaycastLambda = TFunctionRef<void(int32 Payload, TVoxelArrayView<FVector3f> RayPositions, TVoxelArrayView<FVector3f> RayDirections)>;
	void BulkRaycast(
		TConstVoxelArrayView<FVector3f> RayPositions,
		TConstVoxelArrayView<FVector3f> RayDirections,
		FBulkRaycastLambda Lambda);

	template<typename LambdaType>
	bool Raycast(const FVector& RayOrigin, const FVector& RayDirection, LambdaType&& Lambda) const
	{
		if (Nodes.Num() == 0)
		{
			return true;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];
				for (const FElement& Element : Leaf.Elements)
				{
					if (!Element.Bounds.RayBoxIntersection(RayOrigin, RayDirection))
					{
						continue;
					}

					if (!Lambda(Element.Payload))
					{
						return false;
					}
				}
			}
			else
			{
				if (Node.ChildBounds0.RayBoxIntersection(RayOrigin, RayDirection))
				{
					QueuedNodes.Add(Node.ChildIndex0);
				}
				if (Node.ChildBounds1.RayBoxIntersection(RayOrigin, RayDirection))
				{
					QueuedNodes.Add(Node.ChildIndex1);
				}
			}
		}

		return true;
	}
	template<typename LambdaType>
	bool Sweep(const FVector& RayOrigin, const FVector& RayDirection, const FVector& SweepHalfExtents, LambdaType&& Lambda) const
	{
		if (Nodes.Num() == 0)
		{
			return true;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];
				for (const FElement& Element : Leaf.Elements)
				{
					if (!Element.Bounds.Extend(SweepHalfExtents).RayBoxIntersection(RayOrigin, RayDirection))
					{
						continue;
					}

					if (!Lambda(Element.Payload))
					{
						return false;
					}
				}
			}
			else
			{
				if (Node.ChildBounds0.Extend(SweepHalfExtents).RayBoxIntersection(RayOrigin, RayDirection))
				{
					QueuedNodes.Add(Node.ChildIndex0);
				}
				if (Node.ChildBounds1.Extend(SweepHalfExtents).RayBoxIntersection(RayOrigin, RayDirection))
				{
					QueuedNodes.Add(Node.ChildIndex1);
				}
			}
		}

		return true;
	}

	bool Intersect(const FVoxelBox& OverlapBounds) const
	{
		return Overlap(OverlapBounds, [](int32)
		{
			return true;
		});
	}
	template<typename LambdaType>
	bool Overlap(const FVoxelBox& OverlapBounds, LambdaType&& Lambda) const
	{
		if (Nodes.Num() == 0)
		{
			return false;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];
				for (const FElement& Element : Leaf.Elements)
				{
					if (!Element.Bounds.Intersect(OverlapBounds))
					{
						continue;
					}

					if (Lambda(Element.Payload))
					{
						return true;
					}
				}
			}
			else
			{
				if (Node.ChildBounds0.Intersect(OverlapBounds))
				{
					QueuedNodes.Add(Node.ChildIndex0);
				}
				if (Node.ChildBounds1.Intersect(OverlapBounds))
				{
					QueuedNodes.Add(Node.ChildIndex1);
				}
			}
		}

		return false;
	}

	template<typename ShouldVisitType, typename VisitType>
	void Traverse(ShouldVisitType&& ShouldVisit, VisitType&& Visit) const
	{
		if (Nodes.Num() == 0)
		{
			return;
		}

		TVoxelInlineArray<int32, 64> QueuedNodes;
		QueuedNodes.Add(0);

		while (QueuedNodes.Num() > 0)
		{
			const int32 NodeIndex = QueuedNodes.Pop();

			const FNode& Node = Nodes[NodeIndex];
			if (Node.bLeaf)
			{
				const FLeaf& Leaf = Leaves[Node.LeafIndex];
				for (const FElement& Element : Leaf.Elements)
				{
					if (!ShouldVisit(Element.Bounds))
					{
						continue;
					}

					Visit(Element.Payload);
				}
			}
			else
			{
				if (ShouldVisit(Node.ChildBounds0))
				{
					QueuedNodes.Add(Node.ChildIndex0);
				}
				if (ShouldVisit(Node.ChildBounds1))
				{
					QueuedNodes.Add(Node.ChildIndex1);
				}
			}
		}
	}
	template<typename VisitType>
	void TraverseBounds(const FVoxelBox& Bounds, VisitType&& Visit) const
	{
		this->Traverse(
			[&](const FVoxelBox& OtherBounds)
			{
				return OtherBounds.Intersect(Bounds);
			},
			MoveTemp(Visit));
	}

private:
	FVoxelBox RootBounds;
	TVoxelArray<FNode> Nodes;
	TVoxelArray<FLeaf> Leaves;
};