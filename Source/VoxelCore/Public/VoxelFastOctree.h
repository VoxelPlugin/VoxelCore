// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

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
	FORCENOINLINE int32 CreateChild(const FNodeRef NodeRef, const int32 Child)
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
		return ChildIndex;
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

	template<typename VectorType>
	FORCEINLINE FNodeRef FindOrAddChild(const FNodeRef NodeRef, const VectorType Position)
	{
		const int32 Child =
			1 * (Position.X >= NodeRef.Center.X) +
			2 * (Position.Y >= NodeRef.Center.Y) +
			4 * (Position.Z >= NodeRef.Center.Z);

		int32 ChildIndex = IndexToChildren[NodeRef.Index][Child];
		if (ChildIndex == -1)
		{
			ChildIndex = this->CreateChild(NodeRef, Child);
		}
		return FNodeRef(ChildIndex, NodeRef.Height - 1, NodeRef.GetChildCenter(Child));
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
		TVoxelStaticArray<FNodeRef, 8 * MaxDepth> NodesToTraverse{ NoInit };

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
			if (Children[4] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[4], NodeRef.Height - 1, NodeRef.GetChildCenter(4)); }
			if (Children[5] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[5], NodeRef.Height - 1, NodeRef.GetChildCenter(5)); }
			if (Children[6] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[6], NodeRef.Height - 1, NodeRef.GetChildCenter(6)); }
			if (Children[7] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[7], NodeRef.Height - 1, NodeRef.GetChildCenter(7)); }
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
	FORCENOINLINE void TraverseBounds(const FVoxelIntBox& Bounds, LambdaType Lambda) const
	{
		TVoxelStaticArray<FNodeRef, 8 * MaxDepth> NodesToTraverse{ NoInit };

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
			if (Children[4] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[4], NodeRef.Height - 1, NodeRef.GetChildCenter(4)); }
			if (Children[5] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[5], NodeRef.Height - 1, NodeRef.GetChildCenter(5)); }
			if (Children[6] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[6], NodeRef.Height - 1, NodeRef.GetChildCenter(6)); }
			if (Children[7] != -1) { NodesToTraverse[NumNodesToTraverse++] = FNodeRef(Children[7], NodeRef.Height - 1, NodeRef.GetChildCenter(7)); }
		}
	}

	FORCENOINLINE FNodeRef FindOrAddLeaf(const FIntVector& Position)
	{
		FNodeRef NodeRef = Root();

		while (NodeRef.GetHeight() > 0)
		{
			checkVoxelSlow(NodeRef.GetBounds().Contains(Position));

			NodeRef = this->FindOrAddChild(NodeRef, Position);
		}
		checkVoxelSlow(NodeRef.GetHeight() == 0);
		checkVoxelSlow(NodeRef.GetMin() == Position);

		return NodeRef;
	}

private:
	TVoxelSparseArray<FChildren> IndexToChildren;
	TVoxelArray<NodeType> Nodes;
};