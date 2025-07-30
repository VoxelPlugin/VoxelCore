// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelAABBTree
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
		FORCEINLINE int32 Max() const
		{
			checkVoxelSlow(Payload.Max() == MinX.Max());
			checkVoxelSlow(Payload.Max() == MinY.Max());
			checkVoxelSlow(Payload.Max() == MinZ.Max());
			checkVoxelSlow(Payload.Max() == MaxX.Max());
			checkVoxelSlow(Payload.Max() == MaxY.Max());
			checkVoxelSlow(Payload.Max() == MaxZ.Max());
			return Payload.Max();
		}

		FORCEINLINE void Set(
			const int32 WriteIndex,
			const FVoxelBox& Bounds,
			const int32 PayloadIndex)
		{
			MinX[WriteIndex] = FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.X);
			MinY[WriteIndex] = FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.Y);
			MinZ[WriteIndex] = FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.Z);
			MaxX[WriteIndex] = FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.X);
			MaxY[WriteIndex] = FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.Y);
			MaxZ[WriteIndex] = FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.Z);
			Payload[WriteIndex] = PayloadIndex;
		}
		FORCEINLINE void Add(
			const FVoxelBox& Bounds,
			const int32 PayloadIndex)
		{
			ensureVoxelSlow(Bounds.IsValidAndNotEmpty());

			MinX.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.X));
			MinY.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.Y));
			MinZ.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Lower(Bounds.Min.Z));
			MaxX.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.X));
			MaxY.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.Y));
			MaxZ.Add_EnsureNoGrow(FVoxelUtilities::DoubleToFloat_Higher(Bounds.Max.Z));
			Payload.Add_EnsureNoGrow(PayloadIndex);
		}

		void Reserve(const int32 Number)
		{
			// Add some padding to make sure ISPC is safe
			Payload.Reserve(Number + 16);
			MinX.Reserve(Number + 16);
			MinY.Reserve(Number + 16);
			MinZ.Reserve(Number + 16);
			MaxX.Reserve(Number + 16);
			MaxY.Reserve(Number + 16);
			MaxZ.Reserve(Number + 16);
		}
		void SetNum(const int32 Number)
		{
			VOXEL_FUNCTION_COUNTER();

			Reserve(Number);

			Payload.SetNumUninitialized(Number, EAllowShrinking::No);
			MinX.SetNumUninitialized(Number, EAllowShrinking::No);
			MinY.SetNumUninitialized(Number, EAllowShrinking::No);
			MinZ.SetNumUninitialized(Number, EAllowShrinking::No);
			MaxX.SetNumUninitialized(Number, EAllowShrinking::No);
			MaxY.SetNumUninitialized(Number, EAllowShrinking::No);
			MaxZ.SetNumUninitialized(Number, EAllowShrinking::No);
		}
	};

	struct FNode
	{
	public:
		FVoxelFastBox ChildBounds0;
		FVoxelFastBox ChildBounds1;

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
		int32 StartIndex = -1;
		int32 EndIndex = -1;

		FORCEINLINE int32 Num() const
		{
			checkVoxelSlow(StartIndex < EndIndex);
			return EndIndex - StartIndex;
		}
	};

	const int32 MaxChildrenInLeaf;
	const int32 MaxTreeDepth;

	explicit FVoxelAABBTree(
		int32 MaxChildrenInLeaf = 12,
		int32 MaxTreeDepth = 16);

	void Initialize(FElementArray&& Elements);
	void Shrink();

	void DrawTree(
		TVoxelObjectPtr<UWorld> World,
		const FLinearColor& Color,
		const FTransform& Transform,
		int32 Index) const;

	static TSharedRef<FVoxelAABBTree> Create(FElementArray&& Elements);
	static TSharedRef<FVoxelAABBTree> Create(TConstVoxelArrayView<FVoxelBox> Bounds);

public:
	FORCEINLINE int32 Num() const
	{
		return Payloads.Num();
	}
	FORCEINLINE bool IsEmpty() const
	{
		return Payloads.Num() == 0;
	}
	const FVoxelFastBox& GetBounds() const
	{
		ensure(RootBounds.GetBox().IsValidAndNotEmpty());
		return RootBounds;
	}

	FORCEINLINE int32 GetPayload(const int32 Index) const
	{
		return Payloads[Index];
	}
	FORCEINLINE const FVoxelFastBox& GetBounds(const int32 Index) const
	{
		return ElementBounds[Index];
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
	FORCEINLINE bool Intersects(const FVoxelFastBox& Bounds) const
	{
		return Intersects(Bounds, [](int32)
		{
			return true;
		});
	}
	template<typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, bool(int32)>
	FORCEINLINE bool Intersects(
		const FVoxelFastBox& Bounds,
		LambdaType&& CustomCheck) const
	{
		// Critical for performance
		if (!Bounds.Intersects(RootBounds))
		{
			return false;
		}

		return this->IntersectsImpl(
			Bounds,
			MoveTemp(CustomCheck));
	}

private:
	template<typename LambdaType>
	FORCENOINLINE bool IntersectsImpl(
		const FVoxelFastBox& Bounds,
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
				for (int32 Index = Leaf.StartIndex; Index < Leaf.EndIndex; Index++)
				{
					if (!Bounds.Intersects(ElementBounds[Index]))
					{
						continue;
					}

					if (CustomCheck(Payloads[Index]))
					{
						return true;
					}
				}
			}
			else
			{
				if (Bounds.Intersects(Node.ChildBounds0))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex0);
				}
				if (Bounds.Intersects(Node.ChildBounds1))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex1);
				}
			}
		}

		return false;
	}

public:
	template<typename ShouldVisitType, typename VisitType>
	requires
	(
		LambdaHasSignature_V<ShouldVisitType, bool(const FVoxelFastBox&)> &&
		LambdaHasSignature_V<VisitType, void(int32)>
	)
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

				for (int32 Index = Leaf.StartIndex; Index < Leaf.EndIndex; Index++)
				{
					if (!ShouldVisit(ElementBounds[Index]))
					{
						continue;
					}

					Visit(Payloads[Index]);
				}
			}
			else
			{
				if (ShouldVisit(Node.ChildBounds0))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex0);
				}
				if (ShouldVisit(Node.ChildBounds1))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex1);
				}
			}
		}
	}
	template<typename VisitType>
	requires LambdaHasSignature_V<VisitType, void(int32)>
	void TraverseBounds(
		const FVoxelFastBox& Bounds,
		VisitType&& Visit) const
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

				for (int32 Index = Leaf.StartIndex; Index < Leaf.EndIndex; Index++)
				{
					if (!Bounds.Intersects(ElementBounds[Index]))
					{
						continue;
					}

					Visit(Payloads[Index]);
				}
			}
			else
			{
				if (Bounds.Intersects(Node.ChildBounds0))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex0);
				}
				if (Bounds.Intersects(Node.ChildBounds1))
				{
					QueuedNodes.Add_EnsureNoGrow(Node.ChildIndex1);
				}
			}
		}
	}

private:
	FVoxelFastBox RootBounds;
	TVoxelArray<FNode> Nodes;
	TVoxelArray<FLeaf> Leaves;
	TVoxelArray<int32> Payloads;
	TVoxelArray<FVoxelFastBox> ElementBounds;
};