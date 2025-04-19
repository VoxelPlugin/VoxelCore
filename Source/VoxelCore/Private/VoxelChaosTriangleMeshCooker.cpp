// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelChaosTriangleMeshCooker.h"
#include "VoxelFastAABBTree.h"
#include "Chaos/TriangleMeshImplicitObject.h"

VOXEL_CONSOLE_VARIABLE(
	VOXELCORE_API, bool, GVoxelCollisionFastCooking, true,
	"voxel.collision.FastCooking",
	"");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace Chaos
{
	template<typename, typename>
	struct FTriangleMeshSweepVisitorCCD;

	template<>
	struct FTriangleMeshSweepVisitorCCD<void, void>
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
	};

	struct FCookTriangleDummy;

	template<>
	struct FTriangleMeshOverlapVisitorNoMTD<FCookTriangleDummy>
	{
		template<typename IndexType>
		static TRefCountPtr<FTriangleMeshImplicitObject> CookTriangleMesh(
			const TConstVoxelArrayView<int32> Indices,
			const TConstVoxelArrayView<FVector3f> Vertices,
			const TConstVoxelArrayView<uint16> FaceMaterials)
		{
			VOXEL_FUNCTION_COUNTER();
			checkVoxelSlow(Indices.Num() > 0);

			TParticles<FRealSingle, 3> Particles;
			Particles.AddParticles(Vertices.Num());

			for (int32 Index = 0; Index < Vertices.Num(); Index++)
			{
				Particles.SetX(Index, Vertices[Index]);
			}

			const int32 NumTriangles = Indices.Num() / 3;

			TVoxelArray<TVector<IndexType, 3>> Triangles;
			Triangles.Reserve(NumTriangles);

			for (int32 Index = 0; Index < NumTriangles; Index++)
			{
				const TVector<int32, 3> Triangle{
					Indices[3 * Index + 2],
					Indices[3 * Index + 1],
					Indices[3 * Index + 0]
				};

				if (!FConvexBuilder::IsValidTriangle(
					Particles.GetX(Triangle.X),
					Particles.GetX(Triangle.Y),
					Particles.GetX(Triangle.Z)))
				{
					continue;
				}

				Triangles.Add(Triangle);
			}

			if (Triangles.Num() == 0)
			{
				return nullptr;
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

			FVoxelFastAABBTree Tree(MaxChildrenInLeaf, MaxTreeDepth);
			{
				FVoxelFastAABBTree::FElementArray Elements;
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

			const TConstVoxelArrayView<FVoxelFastAABBTree::FNode> SrcNodes = Tree.GetNodes();
			const TConstVoxelArrayView<FVoxelFastAABBTree::FLeaf> SrcLeaves = Tree.GetLeaves();

			FBVHType BVH;
			FVoxelUtilities::SetNum(ConstCast(BVH.GetNodes()), SrcNodes.Num());
			FVoxelUtilities::SetNum(ConstCast(BVH.GetLeaves()), SrcLeaves.Num());

			{
				VOXEL_SCOPE_COUNTER("Copy Nodes");

				const TVoxelArrayView<TAABBTreeNode<float>> DestNodes = MakeVoxelArrayView(ConstCast(BVH.GetNodes()));

				for (int32 Index = 0; Index < SrcNodes.Num(); Index++)
				{
					const FVoxelFastAABBTree::FNode& SrcNode = SrcNodes[Index];
					TAABBTreeNode<float>& DestNode = DestNodes[Index];

					if (SrcNode.bLeaf)
					{
						DestNode.bLeaf = true;
						DestNode.ChildrenNodes[0] = SrcNode.LeafIndex;
					}
					else
					{
						DestNode.bLeaf = false;
						DestNode.ChildrenNodes[0] = SrcNode.ChildIndex0;
						DestNode.ChildrenNodes[1] = SrcNode.ChildIndex1;
						DestNode.ChildrenBounds[0] = FAABB3f(SrcNode.ChildBounds0.GetMin(), SrcNode.ChildBounds0.GetMax());
						DestNode.ChildrenBounds[1] = FAABB3f(SrcNode.ChildBounds1.GetMin(), SrcNode.ChildBounds1.GetMax());
					}
				}
			}

			{
				VOXEL_SCOPE_COUNTER("Copy Leaves");

				const TVoxelArrayView<FLeaf> DestLeaves = MakeVoxelArrayView(ConstCast(BVH.GetLeaves()));

				for (int32 Index = 0; Index < SrcLeaves.Num(); Index++)
				{
					const FVoxelFastAABBTree::FLeaf& SrcLeaf = SrcLeaves[Index];
					FLeaf& DestLeaf = DestLeaves[Index];

					FVoxelUtilities::SetNumFast(DestLeaf.Elems, SrcLeaf.Num());

					const TVoxelArrayView<TPayloadBoundsElement<int32, float>> Elems = MakeVoxelArrayView(DestLeaf.Elems);

					for (int32 ElementIndex = SrcLeaf.StartIndex; ElementIndex < SrcLeaf.EndIndex; ElementIndex++)
					{
						TPayloadBoundsElement<int32, float>& DestElement = Elems[ElementIndex - SrcLeaf.StartIndex];

						DestElement.Payload = Tree.GetPayload(ElementIndex);
						DestElement.Bounds = FAABB3f(
							Tree.GetBounds(ElementIndex).GetMin(),
							Tree.GetBounds(ElementIndex).GetMax());
					}
				}
			}

			VOXEL_SCOPE_COUNTER("FTriangleMeshImplicitObject::FTriangleMeshImplicitObject");

			return new FTriangleMeshImplicitObject(
				MoveTemp(Particles),
				MoveTemp(Triangles),
				TArray<uint16>(FaceMaterials),
				BVH,
				nullptr,
				nullptr,
				true);
		}
	};
}

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

	using FCooker = Chaos::FTriangleMeshOverlapVisitorNoMTD<Chaos::FCookTriangleDummy>;

	if (Vertices.Num() < MAX_uint16)
	{
		return FCooker::CookTriangleMesh<uint16>(Indices, Vertices, FaceMaterials);
	}
	else
	{
		return FCooker::CookTriangleMesh<int32>(Indices, Vertices, FaceMaterials);
	}
}

int64 FVoxelChaosTriangleMeshCooker::GetAllocatedSize(const Chaos::FTriangleMeshImplicitObject& TriangleMesh)
{
	return Chaos::FTriangleMeshSweepVisitorCCD<void, void>::GetAllocatedSize(TriangleMesh);
}