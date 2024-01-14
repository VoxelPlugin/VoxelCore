// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelIntBox.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelSparseArray.h"

struct FVoxelFastOctreeNodeDummy {};

template<typename NodeType = FVoxelFastOctreeNodeDummy>
class TVoxelFastOctree
{
public:
	class FNodeRef
	{
	public:
		FORCEINLINE int32 GetHeight() const
		{
			return Height;
		}
		FORCEINLINE int32 GetSize() const
		{
			return 1 << Height;
		}
		FORCEINLINE FVoxelIntBox GetBounds() const
		{
			const int32 Size = 1 << Height;
			return FVoxelIntBox(
				Center - FVoxelUtilities::DivideFloor_Positive(Size, 2),
				Center + FVoxelUtilities::DivideCeil_Positive(Size, 2));
		}
		FORCEINLINE FIntVector GetCenter() const
		{
			return Center;
		}

		FORCEINLINE FIntVector GetMin() const
		{
			return GetBounds().Min;
		}
		FORCEINLINE FIntVector GetMax() const
		{
			return GetBounds().Max;
		}

		FORCEINLINE bool IsRoot() const
		{
			return Height > 0 && Center == FIntVector(ForceInit);
		}

		static constexpr int32 InvalidIndex = (1 << 24) - 1;

	private:
		uint32 Index : 24;
		uint32 Height : 8;
		// If Height = 0 this is the bottom corner of the node
		FIntVector Center;

		FORCEINLINE FNodeRef(
			const int32 Index,
			const int32 Height,
			const FIntVector& Center)
			: Index(Index)
			, Height(Height)
			, Center(Center)
		{
			checkVoxelSlow(0 <= Index && Index < (1 << 24));
			checkVoxelSlow(0 <= Height && Height < 256);
		}

		FORCEINLINE FIntVector GetChildCenter(const int32 Child) const
		{
			checkVoxelSlow(Height > 0);

			const int32 Size = 1 << Height;
			const int32 NegativeOffset = -(Size + 2) / 4;
			const int32 PositiveOffset = Size / 4;

			checkVoxelSlow(Height != 1 || NegativeOffset == -1);
			checkVoxelSlow(Height != 1 || PositiveOffset == 0);
			checkVoxelSlow(Height == 1 || NegativeOffset == -Size / 4);
			checkVoxelSlow(Height == 1 || PositiveOffset == Size / 4);

			return Center + FIntVector(
				Child & 0x1 ? PositiveOffset : NegativeOffset,
				Child & 0x2 ? PositiveOffset : NegativeOffset,
				Child & 0x4 ? PositiveOffset : NegativeOffset);
		}

		friend TVoxelFastOctree;
	};
	checkStatic(sizeof(FNodeRef) == 16);

	using FChildren = TVoxelStaticArray<int32, 8>;

	static constexpr bool bHasNodes = !std::is_same_v<NodeType, FVoxelFastOctreeNodeDummy>;

	// If Depth == 1 we only have a single root node, which breaks IsRoot
	static constexpr int32 MinDepth = 2;
	static constexpr int32 MaxDepth = 30;

public:
	const int32 Depth;

	explicit TVoxelFastOctree(const int32 Depth)
		: Depth(FMath::Clamp(Depth, MinDepth, MaxDepth))
	{
		ensure(MinDepth <= Depth && Depth <= MaxDepth);

		IndexToChildren.Add(FChildren(-1));

		if (bHasNodes)
		{
			Nodes.Add({});
		}
	}

public:
	void MoveFrom(TVoxelFastOctree& Other)
	{
		ensure(Depth == Other.Depth);
		IndexToChildren = MoveTemp(Other.IndexToChildren);
		Nodes = MoveTemp(Other.Nodes);
	}
	void CopyFrom(const TVoxelFastOctree& Other)
	{
		ensure(Depth == Other.Depth);
		IndexToChildren = Other.IndexToChildren;
		Nodes = Other.Nodes;
	}

public:
	FORCEINLINE int32 NumNodes() const
	{
		return IndexToChildren.Num();
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return IndexToChildren.GetAllocatedSize() + Nodes.GetAllocatedSize();
	}
	FORCEINLINE FNodeRef Root() const
	{
		return FNodeRef(0, Depth - 1, FIntVector(ForceInit));
	}

public:
	FORCEINLINE NodeType& GetNode(const FNodeRef NodeRef)
	{
		checkStatic(bHasNodes);
		return Nodes[NodeRef.Index];
	}
	FORCEINLINE const NodeType& GetNode(const FNodeRef NodeRef) const
	{
		checkStatic(bHasNodes);
		return Nodes[NodeRef.Index];
	}

public:
	void CreateChild(const FNodeRef NodeRef, const int32 Child)
	{
		checkVoxelSlow(NodeRef.Height > 0);
		checkVoxelSlow(0 <= Child && Child < 8);

		const int32 NewChildIndex = IndexToChildren.Add(FChildren(-1));

		if (bHasNodes && !Nodes.IsValidIndex(NewChildIndex))
		{
			Nodes.SetNum(NewChildIndex + 1);
		}

		int32& ChildIndex = IndexToChildren[NodeRef.Index][Child];
		checkVoxelSlow(ChildIndex == -1);
		ChildIndex = NewChildIndex;
	}
	FORCENOINLINE void DestroyChild(const FNodeRef NodeRef, const int32 Child)
	{
		checkVoxelSlow(NodeRef.Height > 0);
		checkVoxelSlow(0 <= Child && Child < 8);

		const int32 ChildIndex = IndexToChildren[NodeRef.Index][Child];
		checkVoxelSlow(ChildIndex != -1);

		for (int32 ChildChild = 0; ChildChild < 8; ChildChild++)
		{
			if (IndexToChildren[ChildIndex][ChildChild] != -1)
			{
				const FNodeRef ChildNodeRef(ChildIndex, NodeRef.Height - 1, NodeRef.GetChildCenter(ChildChild));
				this->DestroyChild(ChildNodeRef, ChildChild);
			}
		}

		if (bHasNodes)
		{
			Nodes[ChildIndex] = {};
		}

		IndexToChildren.RemoveAt(ChildIndex);
		IndexToChildren[NodeRef.Index][Child] = -1;
	}

	template<typename VectorType>
	FORCEINLINE bool TryGetChild(const FNodeRef NodeRef, const VectorType Position, FNodeRef& OutChildNodeRef) const
	{
		const int32 Child =
			1 * (Position.X >= NodeRef.Center.X) +
			2 * (Position.Y >= NodeRef.Center.Y) +
			4 * (Position.Z >= NodeRef.Center.Z);

		const int32 ChildIndex = IndexToChildren[NodeRef.Index][Child];
		if (ChildIndex == -1)
		{
			return false;
		}

		OutChildNodeRef = FNodeRef(ChildIndex, NodeRef.Height - 1, NodeRef.GetChildCenter(Child));
		return true;
	}

public:
	FORCEINLINE bool HasAnyChildren(const FNodeRef NodeRef) const
	{
		const FChildren& Children = IndexToChildren[NodeRef.Index];
		return
			Children[0] != -1 ||
			Children[1] != -1 ||
			Children[2] != -1 ||
			Children[3] != -1 ||
			Children[4] != -1 ||
			Children[5] != -1 ||
			Children[6] != -1 ||
			Children[7] != -1;
	}
	void CreateAllChildren(const FNodeRef NodeRef)
	{
		const FChildren Children = IndexToChildren[NodeRef.Index];
		for (int32 Index = 0; Index < 8; Index++)
		{
			if (Children[Index] == -1)
			{
				this->CreateChild(NodeRef, Index);
			}
		}
	}
	void DestroyAllChildren(const FNodeRef NodeRef)
	{
		const FChildren Children = IndexToChildren[NodeRef.Index];
		for (int32 Index = 0; Index < 8; Index++)
		{
			if (Children[Index] != -1)
			{
				this->DestroyChild(NodeRef, Index);
			}
		}
	}

public:
	template<typename LambdaType>
	FORCENOINLINE void Traverse(const FNodeRef InNodeRef, LambdaType Lambda) const
	{
		TVoxelStaticArray<FNodeRef, 8 * MaxDepth> NodesToTraverse{ NoInit };

		int32 NumNodesToTraverse = 1;
		NodesToTraverse[0] = InNodeRef;

		while (NumNodesToTraverse > 0)
		{
			const FNodeRef NodeRef = NodesToTraverse[--NumNodesToTraverse];

			if constexpr (std::is_same_v<decltype(Lambda(DeclVal<FNodeRef>())), void>)
			{
				Lambda(NodeRef);
			}
			else
			{
				if (!Lambda(NodeRef))
				{
					continue;
				}
			}

			if (NodeRef.Height == 0)
			{
				continue;
			}

			const FChildren& Children = IndexToChildren[NodeRef.Index];

			if (Children[0] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[0], NodeRef.Height - 1, NodeRef.GetChildCenter(0)); }
			if (Children[1] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[1], NodeRef.Height - 1, NodeRef.GetChildCenter(1)); }
			if (Children[2] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[2], NodeRef.Height - 1, NodeRef.GetChildCenter(2)); }
			if (Children[3] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[3], NodeRef.Height - 1, NodeRef.GetChildCenter(3)); }
			if (Children[4] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[4], NodeRef.Height - 1, NodeRef.GetChildCenter(4)); }
			if (Children[5] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[5], NodeRef.Height - 1, NodeRef.GetChildCenter(5)); }
			if (Children[6] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[6], NodeRef.Height - 1, NodeRef.GetChildCenter(6)); }
			if (Children[7] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[7], NodeRef.Height - 1, NodeRef.GetChildCenter(7)); }
		}
	}
	template<typename LambdaType>
	FORCEINLINE void Traverse(LambdaType Lambda) const
	{
		this->Traverse(Root(), Lambda);
	}
	template<typename LambdaType>
	FORCEINLINE void TraverseChildren(const FNodeRef NodeRef, LambdaType Lambda) const
	{
		const FChildren Children = IndexToChildren[NodeRef.Index];

		if (Children[0] != -1) { this->Traverse(FNodeRef(Children[0], NodeRef.Height - 1, NodeRef.GetChildCenter(0)), Lambda); }
		if (Children[1] != -1) { this->Traverse(FNodeRef(Children[1], NodeRef.Height - 1, NodeRef.GetChildCenter(1)), Lambda); }
		if (Children[2] != -1) { this->Traverse(FNodeRef(Children[2], NodeRef.Height - 1, NodeRef.GetChildCenter(2)), Lambda); }
		if (Children[3] != -1) { this->Traverse(FNodeRef(Children[3], NodeRef.Height - 1, NodeRef.GetChildCenter(3)), Lambda); }
		if (Children[4] != -1) { this->Traverse(FNodeRef(Children[4], NodeRef.Height - 1, NodeRef.GetChildCenter(4)), Lambda); }
		if (Children[5] != -1) { this->Traverse(FNodeRef(Children[5], NodeRef.Height - 1, NodeRef.GetChildCenter(5)), Lambda); }
		if (Children[6] != -1) { this->Traverse(FNodeRef(Children[6], NodeRef.Height - 1, NodeRef.GetChildCenter(6)), Lambda); }
		if (Children[7] != -1) { this->Traverse(FNodeRef(Children[7], NodeRef.Height - 1, NodeRef.GetChildCenter(7)), Lambda); }
	}
	template<typename LambdaType>
	FORCENOINLINE void TraverseBounds(const FVoxelIntBox& Bounds, LambdaType Lambda) const
	{
		TVoxelStaticArray<FNodeRef, 8 * MaxDepth> NodesToTraverse{ NoInit };

		int32 NumNodesToTraverse = 1;
		NodesToTraverse[0] = Root();

		while (NumNodesToTraverse > 0)
		{
			const FNodeRef NodeRef = NodesToTraverse[--NumNodesToTraverse];
			if (!NodeRef.GetBounds().Intersect(Bounds))
			{
				continue;
			}

			if constexpr (std::is_same_v<decltype(Lambda(DeclVal<FNodeRef>())), void>)
			{
				Lambda(NodeRef);
			}
			else
			{
				if (!Lambda(NodeRef))
				{
					continue;
				}
			}

			if (NodeRef.Height == 0)
			{
				continue;
			}

			const FChildren& Children = IndexToChildren[NodeRef.Index];

			if (Children[0] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[0], NodeRef.Height - 1, NodeRef.GetChildCenter(0)); }
			if (Children[1] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[1], NodeRef.Height - 1, NodeRef.GetChildCenter(1)); }
			if (Children[2] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[2], NodeRef.Height - 1, NodeRef.GetChildCenter(2)); }
			if (Children[3] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[3], NodeRef.Height - 1, NodeRef.GetChildCenter(3)); }
			if (Children[4] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[4], NodeRef.Height - 1, NodeRef.GetChildCenter(4)); }
			if (Children[5] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[5], NodeRef.Height - 1, NodeRef.GetChildCenter(5)); }
			if (Children[6] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[6], NodeRef.Height - 1, NodeRef.GetChildCenter(6)); }
			if (Children[7] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[7], NodeRef.Height - 1, NodeRef.GetChildCenter(7)); }
		}
	}

public:
	template<typename PredicateType, typename AddNodeType, typename RemoveNodeType>
	void Update(
		const FNodeRef NodeRef,
		const PredicateType& Predicate,
		const AddNodeType& AddNode,
		const RemoveNodeType& RemoveNode)
	{
		if (NodeRef.Height == 0)
		{
			return;
		}

		for (int32 Child = 0; Child < 8; Child++)
		{
			if (IndexToChildren[NodeRef.Index][Child] == -1)
			{
				const FNodeRef DummyChildNodeRef(FNodeRef::InvalidIndex, NodeRef.Height - 1, NodeRef.GetChildCenter(Child));
				if (!Predicate(DummyChildNodeRef))
				{
					continue;
				}

				this->CreateChild(NodeRef, Child);

				const FNodeRef ChildNodeRef(IndexToChildren[NodeRef.Index][Child], NodeRef.Height - 1, NodeRef.GetChildCenter(Child));
				AddNode(ChildNodeRef);
				this->Update(ChildNodeRef, Predicate, AddNode, RemoveNode);
			}
			else
			{
				const FNodeRef ChildNodeRef(IndexToChildren[NodeRef.Index][Child], NodeRef.Height - 1, NodeRef.GetChildCenter(Child));
				this->Update(ChildNodeRef, Predicate, AddNode, RemoveNode);

				if (Predicate(ChildNodeRef))
				{
					continue;
				}

				ensureVoxelSlowNoSideEffects(!this->HasAnyChildren(ChildNodeRef));
				RemoveNode(ChildNodeRef);
				this->DestroyChild(NodeRef, Child);
			}
		}
	}
	template<typename PredicateType, typename AddNodeType, typename RemoveNodeType>
	void Update(
		const PredicateType& Predicate,
		const AddNodeType& AddNode,
		const RemoveNodeType& RemoveNode)
	{
		this->Update(Root(), Predicate, AddNode, RemoveNode);
	}

private:
	TVoxelSparseArray<FChildren> IndexToChildren;
	TVoxelArray<NodeType> Nodes;
};