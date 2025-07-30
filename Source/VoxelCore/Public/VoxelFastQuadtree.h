// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

struct FVoxelFastQuadtreeNodeDummy {};

template<typename NodeType = FVoxelFastQuadtreeNodeDummy>
class TVoxelFastQuadtree
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
		FORCEINLINE FVoxelIntBox2D GetBounds() const
		{
			const int32 Size = 1 << Height;
			return FVoxelIntBox2D(
				Center - FVoxelUtilities::DivideFloor_Positive(Size, 2),
				Center + FVoxelUtilities::DivideCeil_Positive(Size, 2));
		}
		FORCEINLINE FIntPoint GetCenter() const
		{
			return Center;
		}

		FORCEINLINE FIntPoint GetMin() const
		{
			return GetBounds().Min;
		}
		FORCEINLINE FIntPoint GetMax() const
		{
			return GetBounds().Max;
		}

		FORCEINLINE bool IsRoot() const
		{
			return Height > 0 && Center == FIntPoint(ForceInit);
		}

		static constexpr int32 InvalidIndex = (1 << 24) - 1;

	private:
		uint32 Index : 24;
		uint32 Height : 8;
		// If Height = 0 this is the bottom corner of the node
		FIntPoint Center;

		FORCEINLINE FNodeRef(
			const int32 Index,
			const int32 Height,
			const FIntPoint& Center)
			: Index(Index)
			, Height(Height)
			, Center(Center)
		{
			checkVoxelSlow(0 <= Index && Index < (1 << 24));
			checkVoxelSlow(0 <= Height && Height < 256);
		}

		FORCEINLINE FIntPoint GetChildCenter(const int32 Child) const
		{
			checkVoxelSlow(Height > 0);

			const int32 Size = 1 << Height;
			const int32 NegativeOffset = -(Size + 2) / 4;
			const int32 PositiveOffset = Size / 4;

			checkVoxelSlow(Height != 1 || NegativeOffset == -1);
			checkVoxelSlow(Height != 1 || PositiveOffset == 0);
			checkVoxelSlow(Height == 1 || NegativeOffset == -Size / 4);
			checkVoxelSlow(Height == 1 || PositiveOffset == Size / 4);

			return Center + FIntPoint(
				Child & 0x1 ? PositiveOffset : NegativeOffset,
				Child & 0x2 ? PositiveOffset : NegativeOffset);
		}

		friend TVoxelFastQuadtree;
	};
	checkStatic(sizeof(FNodeRef) == 12);

	using FChildren = TVoxelStaticArray<int32, 4>;

	static constexpr bool bHasNodes = !std::is_same_v<NodeType, FVoxelFastQuadtreeNodeDummy>;

	// If Depth == 1 we only have a single root node, which breaks IsRoot
	static constexpr int32 MinDepth = 2;
	static constexpr int32 MaxDepth = 30;

public:
	const int32 Depth;

	explicit TVoxelFastQuadtree(const int32 Depth)
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
	void MoveFrom(TVoxelFastQuadtree& Other)
	{
		ensure(Depth == Other.Depth);
		IndexToChildren = MoveTemp(Other.IndexToChildren);
		Nodes = MoveTemp(Other.Nodes);
	}
	void CopyFrom(const TVoxelFastQuadtree& Other)
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
		return FNodeRef(0, Depth - 1, FIntPoint(ForceInit));
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
		checkVoxelSlow(0 <= Child && Child < 4);

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
		checkVoxelSlow(0 <= Child && Child < 4);

		const int32 ChildIndex = IndexToChildren[NodeRef.Index][Child];
		checkVoxelSlow(ChildIndex != -1);

		for (int32 ChildChild = 0; ChildChild < 4; ChildChild++)
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
			2 * (Position.Y >= NodeRef.Center.Y);

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
			Children[3] != -1;
	}
	void CreateAllChildren(const FNodeRef NodeRef)
	{
		const FChildren Children = IndexToChildren[NodeRef.Index];
		for (int32 Index = 0; Index < 4; Index++)
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
		for (int32 Index = 0; Index < 4; Index++)
		{
			if (Children[Index] != -1)
			{
				this->DestroyChild(NodeRef, Index);
			}
		}
	}

public:
	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterateTree>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FNodeRef&)>
	)
	FORCENOINLINE void Traverse(const FNodeRef InNodeRef, LambdaType Lambda) const
	{
		TVoxelStaticArray<FNodeRef, 4 * MaxDepth> NodesToTraverse{ NoInit };

		int32 NumNodesToTraverse = 1;
		NodesToTraverse[0] = InNodeRef;

		while (NumNodesToTraverse > 0)
		{
			const FNodeRef NodeRef = NodesToTraverse[--NumNodesToTraverse];

			if constexpr (std::is_void_v<ReturnType>)
			{
				Lambda(NodeRef);
			}
			else
			{
				switch (Lambda(NodeRef))
				{
				default: VOXEL_ASSUME(false);
				case EVoxelIterateTree::Continue: break;
				case EVoxelIterateTree::SkipChildren: continue;
				case EVoxelIterateTree::Stop: return;
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
		}
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterateTree>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FNodeRef&)>
	)
	FORCEINLINE void Traverse(LambdaType Lambda) const
	{
		this->Traverse(Root(), Lambda);
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterateTree>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FNodeRef&)>
	)
	FORCEINLINE void TraverseChildren(const FNodeRef& NodeRef, LambdaType Lambda) const
	{
		const FChildren Children = IndexToChildren[NodeRef.Index];

		if (Children[0] != -1) { this->Traverse(FNodeRef(Children[0], NodeRef.Height - 1, NodeRef.GetChildCenter(0)), Lambda); }
		if (Children[1] != -1) { this->Traverse(FNodeRef(Children[1], NodeRef.Height - 1, NodeRef.GetChildCenter(1)), Lambda); }
		if (Children[2] != -1) { this->Traverse(FNodeRef(Children[2], NodeRef.Height - 1, NodeRef.GetChildCenter(2)), Lambda); }
		if (Children[3] != -1) { this->Traverse(FNodeRef(Children[3], NodeRef.Height - 1, NodeRef.GetChildCenter(3)), Lambda); }
	}

	template<typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
	requires
	(
		(std::is_void_v<ReturnType> || std::is_same_v<ReturnType, EVoxelIterateTree>) &&
		LambdaHasSignature_V<LambdaType, ReturnType(const FNodeRef&)>
	)
	FORCENOINLINE void TraverseBounds(const FVoxelIntBox2D& Bounds, LambdaType Lambda) const
	{
		TVoxelStaticArray<FNodeRef, 4 * MaxDepth> NodesToTraverse{ NoInit };

		int32 NumNodesToTraverse = 1;
		NodesToTraverse[0] = Root();

		while (NumNodesToTraverse > 0)
		{
			const FNodeRef NodeRef = NodesToTraverse[--NumNodesToTraverse];
			if (!NodeRef.GetBounds().Intersects(Bounds))
			{
				continue;
			}

			if constexpr (std::is_void_v<ReturnType>)
			{
				Lambda(NodeRef);
			}
			else
			{
				switch (Lambda(NodeRef))
				{
				default: VOXEL_ASSUME(false);
				case EVoxelIterateTree::Continue: break;
				case EVoxelIterateTree::SkipChildren: continue;
				case EVoxelIterateTree::Stop: return;
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

		for (int32 Child = 0; Child < 4; Child++)
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