// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelAABBTree2D
{
public:
	struct FElement
	{
		FVoxelBox2D Bounds;
		int32 Payload = -1;
	};
	struct FNode
	{
		FVoxelBox2D ChildBounds0;
		FVoxelBox2D ChildBounds1;

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

	explicit FVoxelAABBTree2D(
		const int32 MaxChildrenInLeaf = 12,
		const int32 MaxTreeDepth = 16)
		: MaxChildrenInLeaf(MaxChildrenInLeaf)
		, MaxTreeDepth(MaxTreeDepth)
	{
	}

	void Initialize(TVoxelArray<FElement>&& Elements);
	void Shrink();

public:
	FORCEINLINE bool IsEmpty() const
	{
		return Nodes.Num() == 0;
	}
	FORCEINLINE const FVoxelBox2D& GetBounds() const
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
	bool Intersects(const FVoxelBox2D& Bounds) const
	{
		return Intersects(Bounds, [](int32)
		{
			return true;
		});
	}
	template<typename LambdaType>
	bool Intersects(
		const FVoxelBox2D& Bounds,
		LambdaType&& CustomCheck) const
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
					if (!Element.Bounds.Intersects(Bounds))
					{
						continue;
					}

					if (CustomCheck(Element.Payload))
					{
						return true;
					}
				}
			}
			else
			{
				if (Node.ChildBounds0.Intersects(Bounds))
				{
					QueuedNodes.Add(Node.ChildIndex0);
				}
				if (Node.ChildBounds1.Intersects(Bounds))
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
	void TraverseBounds(const FVoxelBox2D& Bounds, VisitType&& Visit) const
	{
		this->Traverse(
			[&](const FVoxelBox2D& OtherBounds)
			{
				return OtherBounds.Intersects(Bounds);
			},
			MoveTemp(Visit));
	}

private:
	FVoxelBox2D RootBounds;
	TVoxelArray<FNode> Nodes;
	TVoxelArray<FLeaf> Leaves;
};