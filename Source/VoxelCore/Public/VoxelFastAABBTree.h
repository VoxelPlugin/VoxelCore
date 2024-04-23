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

private:
	TVoxelArray<FNode> Nodes;
	TVoxelArray<FLeaf> Leaves;
	FElementArray Elements;
};