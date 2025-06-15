// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelChaosTriangleMeshCooker.h"
#include "VoxelAABBTree.h"
#include "Chaos/TriangleMeshImplicitObject.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelCollisionFastCooking, true,
	"voxel.collision.FastCooking",
	"Custom cooking for Chaos collision meshes");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_PRIVATE_ACCESS(Chaos::FTrimeshBVH::FChildData, Bounds);

template<>
struct Chaos::FTriangleMeshOverlapVisitorNoMTD<void>
{
	static int64 GetAllocatedSize(const FTriangleMeshImplicitObject& TriangleMesh)
	{
		int64 AllocatedSize = sizeof(FTriangleMeshImplicitObject);
		AllocatedSize += TriangleMesh.MParticles.GetAllocatedSize();

		if (TriangleMesh.MElements.RequiresLargeIndices())
		{
			AllocatedSize += TriangleMesh.MElements.GetLargeIndexBuffer().GetAllocatedSize();
		}
		else
		{
			AllocatedSize += TriangleMesh.MElements.GetSmallIndexBuffer().GetAllocatedSize();
		}

		AllocatedSize += TriangleMesh.MaterialIndices.GetAllocatedSize();

		if (TriangleMesh.ExternalFaceIndexMap)
		{
			AllocatedSize += TriangleMesh.ExternalFaceIndexMap->GetAllocatedSize();
		}
		if (TriangleMesh.ExternalVertexIndexMap)
		{
			AllocatedSize += TriangleMesh.ExternalVertexIndexMap->GetAllocatedSize();
		}

		AllocatedSize += TriangleMesh.FastBVH.Nodes.GetAllocatedSize();
		AllocatedSize += TriangleMesh.FastBVH.FaceBounds.GetAllocatedSize();

		return AllocatedSize;
	}

	template<bool bUseLargeIndices>
	static TRefCountPtr<FTriangleMeshImplicitObject> CookTriangleMesh(
		const TConstVoxelArrayView<int32> Indices,
		const TConstVoxelArrayView<FVector3f> Vertices,
		const TConstVoxelArrayView<uint16> FaceMaterials)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Vertices.Num());
		checkVoxelSlow(Indices.Num() > 0);

		using IndexType = std::conditional_t<bUseLargeIndices, int32, uint16>;

		TParticles<FRealSingle, 3> Particles;
		{
			VOXEL_SCOPE_COUNTER("Build particles");

			Particles.AddParticles(Vertices.Num());

			FVoxelUtilities::Memcpy(
				MakeVoxelArrayView(Particles.XArray()).ReinterpretAs<FVector3f>(),
				Vertices);
		}

		TVoxelArray<TVector<IndexType, 3>> Triangles;
		{
			VOXEL_SCOPE_COUNTER("Build triangles");

			checkVoxelSlow(Indices.Num() % 3 == 0);
			const int32 NumTriangles = Indices.Num() / 3;

			Triangles.Reserve(NumTriangles);

			for (int32 Index = 0; Index < NumTriangles; Index++)
			{
				const TVector<int32, 3> Triangle
				{
					Indices[3 * Index + 2],
					Indices[3 * Index + 1],
					Indices[3 * Index + 0]
				};

				const FVector3f VertexA = Vertices[Triangle.X];
				const FVector3f VertexB = Vertices[Triangle.Y];
				const FVector3f VertexC = Vertices[Triangle.Z];

				checkVoxelSlow(
					FConvexBuilder::IsValidTriangle(
						VertexA,
						VertexB,
						VertexC) ==
					FVoxelUtilities::IsTriangleValid(
						FVector(VertexA),
						FVector(VertexB),
						FVector(VertexC)));

				if (!FVoxelUtilities::IsTriangleValid(
					FVector(VertexA),
					FVector(VertexB),
					FVector(VertexC)))
				{
					continue;
				}

				Triangles.Add_EnsureNoGrow(Triangle);
			}
		}

		if (Triangles.Num() == 0)
		{
			return {};
		}

		if (!GVoxelCollisionFastCooking)
		{
			VOXEL_SCOPE_COUNTER("Slow cook");

			return new FTriangleMeshImplicitObject(
				MoveTemp(Particles),
				MoveTemp(Triangles),
				TArray<uint16>(FaceMaterials),
				nullptr,
				nullptr,
				true);
		}

		VOXEL_SCOPE_COUNTER("Fast cook");

		using FLeaf = TAABBTreeLeafArray<int32, false, float>;
		using FBVHType = TAABBTree<int32, FLeaf, false, float>;
		checkStatic(std::is_same_v<FBVHType, FTriangleMeshImplicitObject::BVHType>);

		// From FTriangleMeshImplicitObject::RebuildBVImp
		constexpr static int32 MaxChildrenInLeaf = 22;
		constexpr static int32 MaxTreeDepth = FBVHType::DefaultMaxTreeDepth;

		FVoxelAABBTree Tree(MaxChildrenInLeaf, MaxTreeDepth);
		{
			FVoxelAABBTree::FElementArray Elements;
			Elements.SetNum(Triangles.Num());
			{
				VOXEL_SCOPE_COUNTER("Build Elements");

				for (int32 Index = 0; Index < Triangles.Num(); Index++)
				{
					const TVector<IndexType, 3>& Triangle = Triangles[Index];
					const FVector3f VertexA = Vertices[Triangle.X];
					const FVector3f VertexB = Vertices[Triangle.Y];
					const FVector3f VertexC = Vertices[Triangle.Z];

					Elements.Payload[Index] = Index;

					Elements.MinX[Index] = FMath::Min3(VertexA.X, VertexB.X, VertexC.X);
					Elements.MinY[Index] = FMath::Min3(VertexA.Y, VertexB.Y, VertexC.Y);
					Elements.MinZ[Index] = FMath::Min3(VertexA.Z, VertexB.Z, VertexC.Z);

					Elements.MaxX[Index] = FMath::Max3(VertexA.X, VertexB.X, VertexC.X);
					Elements.MaxY[Index] = FMath::Max3(VertexA.Y, VertexB.Y, VertexC.Y);
					Elements.MaxZ[Index] = FMath::Max3(VertexA.Z, VertexB.Z, VertexC.Z);
				}
			}
			Tree.Initialize(MoveTemp(Elements));
		}

		VOXEL_SCOPE_COUNTER("FTriangleMeshImplicitObject");

		const TRefCountPtr<FTriangleMeshImplicitObject> Result = new FTriangleMeshImplicitObject();

		// Apply EImplicitObject::DisableCollisions
		Result->bDoCollide = false;

		Result->MParticles = MoveTemp(Particles);

		Result->MLocalBoundingBox = FAABB3(
			Tree.GetBounds().GetBox().Min,
			Tree.GetBounds().GetBox().Max);

		Result->bCullsBackFaceRaycast = true;

		INLINE_LAMBDA
		{
			VOXEL_SCOPE_COUNTER("Build FastBVH");

			const TConstVoxelArrayView<FVoxelAABBTree::FNode> Nodes = Tree.GetNodes();
			const TConstVoxelArrayView<FVoxelAABBTree::FLeaf> Leaves = Tree.GetLeaves();

			const bool bHasMaterialIndices = FaceMaterials.Num() > 0;

			TVoxelArray<TVec3<IndexType>> NewTriangles;
			NewTriangles.Reserve(Triangles.Num());

			TVoxelArray<uint16> NewFaceMaterials;
			NewFaceMaterials.Reserve(FaceMaterials.Num());

			TVoxelArray<FVoxelFastBox> TriangleBounds;
			TriangleBounds.Reserve(Triangles.Num());

			TVoxelArray<FTrimeshBVH::FNode> NewNodes;
			NewNodes.Reserve(Nodes.Num() - Leaves.Num());

			ON_SCOPE_EXIT
			{
				Result->MElements.Reinitialize(MoveTemp(NewTriangles));
				Result->MaterialIndices = MoveTemp(NewFaceMaterials);
				Result->FastBVH.FaceBounds = MoveTemp(ReinterpretCastVoxelArray<FAABBVectorized>(TriangleBounds));
				Result->FastBVH.Nodes = MoveTemp(NewNodes);
			};

			// We're skipping leaf nodes below, handle the special case of having a single leaf node manually
			if (Nodes.Num() == 1)
			{
				const FVoxelAABBTree::FNode& RootNode = Nodes[0];
				checkVoxelSlow(RootNode.bLeaf);

				const FVoxelAABBTree::FLeaf& Leaf = Leaves[RootNode.LeafIndex];
				checkVoxelSlow(Leaf.Num() > 0);

				FTrimeshBVH::FNode& NewNode = NewNodes.Emplace_GetRef();
				NewNode.Children.SetChildOrFaceIndex(0, 0);
				NewNode.Children.SetFaceCount(0, Leaf.Num());
				NewNode.Children.SetBounds(0, Result->BoundingBox());

				for (int32 Index = Leaf.StartIndex; Index < Leaf.EndIndex; Index++)
				{
					const int32 TriangleIndex = Tree.GetPayload(Index);

					TriangleBounds.Add_EnsureNoGrow(Tree.GetBounds(Index));
					NewTriangles.Add_EnsureNoGrow(Triangles[TriangleIndex]);

					if (bHasMaterialIndices)
					{
						NewFaceMaterials.Add(FaceMaterials[TriangleIndex]);
					}
				}

				return;
			}
			checkVoxelSlow(Nodes.Num() > 0);

			TVoxelArray<int32> OldToNewNodeIndex;
			FVoxelUtilities::SetNumFast(OldToNewNodeIndex, Nodes.Num());
			FVoxelUtilities::SetAll(OldToNewNodeIndex, -1);

			TVoxelArray<int32> NodesToVisit;
			NodesToVisit.Reserve(Nodes.Num());
			NodesToVisit.Add_EnsureNoGrow(0);

			while (NodesToVisit.Num() > 0)
			{
				const int32 NodeIndex = NodesToVisit.Pop();
				const FVoxelAABBTree::FNode& Node = Nodes[NodeIndex];
				checkVoxelSlow(!Node.bLeaf);

				checkVoxelSlow(OldToNewNodeIndex[NodeIndex] == -1);
				OldToNewNodeIndex[NodeIndex] = NewNodes.Num();

				FTrimeshBVH::FNode& NewNode = NewNodes.Emplace_GetRef_EnsureNoGrow();

				FTrimeshBVH::FChildData& ChildData = NewNode.Children;
				PrivateAccess::Bounds(ChildData)[0] = ReinterpretCastRef<FAABBVectorized>(Node.ChildBounds0);
				PrivateAccess::Bounds(ChildData)[1] = ReinterpretCastRef<FAABBVectorized>(Node.ChildBounds1);

				const auto AddChildNode = [&]<int32 ChildIndex>()
				{
					const int32 ChildNodeIndex = ChildIndex == 0 ? Node.ChildIndex0 : Node.ChildIndex1;
					const FVoxelAABBTree::FNode& ChildNode = Nodes[ChildNodeIndex];

					if (!ChildNode.bLeaf)
					{
						// ChildIndex will be fixed up below
						ChildData.SetChildOrFaceIndex(ChildIndex, ChildNodeIndex);

						NodesToVisit.Add_EnsureNoGrow(ChildNodeIndex);
						return;
					}

					const FVoxelAABBTree::FLeaf& Leaf = Leaves[ChildNode.LeafIndex];
					checkVoxelSlow(Leaf.Num() > 0);

					ChildData.SetChildOrFaceIndex(ChildIndex, NewTriangles.Num());
					ChildData.SetFaceCount(ChildIndex, Leaf.Num());

					for (int32 Index = Leaf.StartIndex; Index < Leaf.EndIndex; Index++)
					{
						const int32 TriangleIndex = Tree.GetPayload(Index);

						TriangleBounds.Add_EnsureNoGrow(Tree.GetBounds(Index));
						NewTriangles.Add_EnsureNoGrow(Triangles[TriangleIndex]);

						if (bHasMaterialIndices)
						{
							NewFaceMaterials.Add(FaceMaterials[TriangleIndex]);
						}
					}
				};

				AddChildNode.template operator()<0>();
				AddChildNode.template operator()<1>();
			}

			for (FTrimeshBVH::FNode& Node : NewNodes)
			{
				FTrimeshBVH::FChildData& ChildData = Node.Children;

				for (int32 ChildIndex = 0; ChildIndex < 2; ChildIndex++)
				{
					if (ChildData.GetFaceCount(ChildIndex) > 0)
					{
						// Leaf
						continue;
					}

					const int32 OldIndex = ChildData.GetChildOrFaceIndex(ChildIndex);
					const int32 NewIndex = OldToNewNodeIndex[OldIndex];
					checkVoxelSlow(NewIndex != -1);

					ChildData.SetChildOrFaceIndex(ChildIndex, NewIndex);
				}
			}
		};

		return Result;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TRefCountPtr<Chaos::FTriangleMeshImplicitObject> FVoxelChaosTriangleMeshCooker::Create(
	const TConstVoxelArrayView<int32> Indices,
	const TConstVoxelArrayView<FVector3f> Vertices,
	const TConstVoxelArrayView<uint16> FaceMaterials)
{
	VOXEL_FUNCTION_COUNTER();
	ensure(FaceMaterials.Num() == 0 || FaceMaterials.Num() == Indices.Num() / 3);

	if (Indices.Num() == 0 ||
		!ensure(Indices.Num() % 3 == 0))
	{
		return nullptr;
	}

	using FCooker = Chaos::FTriangleMeshOverlapVisitorNoMTD<void>;

	if (Vertices.Num() < MAX_uint16)
	{
		return FCooker::CookTriangleMesh<false>(Indices, Vertices, FaceMaterials);
	}
	else
	{
		return FCooker::CookTriangleMesh<true>(Indices, Vertices, FaceMaterials);
	}
}

int64 FVoxelChaosTriangleMeshCooker::GetAllocatedSize(const Chaos::FTriangleMeshImplicitObject& TriangleMesh)
{
	using FCooker = Chaos::FTriangleMeshOverlapVisitorNoMTD<void>;

	return FCooker::GetAllocatedSize(TriangleMesh);
}