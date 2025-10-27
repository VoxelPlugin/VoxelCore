// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelMap.h"

namespace FVoxelUtilities
{
	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename PredicateType,
		typename IterateType>
	requires LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*)>
	void IterateSortedMaps(
		const TVoxelMap<KeyType, ValueType, Allocator>& MapA,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapB,
		const PredicateType Less,
		const IterateType Iterate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(FMath::Max(MapA.Num(), MapB.Num()), 128);
		checkVoxelSlow(MapA.AreKeySorted(Less));
		checkVoxelSlow(MapB.AreKeySorted(Less));

		using FElement = TVoxelMapElement<KeyType, ValueType>;

		const TConstVoxelArrayView<FElement> ElementsA = MapA.GetElements();
		const TConstVoxelArrayView<FElement> ElementsB = MapB.GetElements();

		int32 IndexA = 0;
		int32 IndexB = 0;
		while (
			IndexA < ElementsA.Num() &&
			IndexB < ElementsB.Num())
		{
			const FElement& A = ElementsA[IndexA];
			const FElement& B = ElementsB[IndexB];

			if (Less(A.Key, B.Key))
			{
				Iterate(A.Key, &A.Value, nullptr);
				IndexA++;
			}
			else if (Less(B.Key, A.Key))
			{
				Iterate(B.Key, nullptr, &B.Value);
				IndexB++;
			}
			else
			{
				checkVoxelSlow(A.Key == B.Key);

				Iterate(A.Key, &A.Value, &B.Value);
				IndexA++;
				IndexB++;
			}
		}

		while (IndexA < ElementsA.Num())
		{
			Iterate(ElementsA[IndexA].Key, &ElementsA[IndexA].Value, nullptr);
			IndexA++;
		}

		while (IndexB < ElementsB.Num())
		{
			Iterate(ElementsB[IndexB].Key, nullptr, &ElementsB[IndexB].Value);
			IndexB++;
		}

		checkVoxelSlow(IndexA == ElementsA.Num());
		checkVoxelSlow(IndexB == ElementsB.Num());
	}

	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename IterateType>
	requires LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*)>
	void IterateSortedMaps(
		const TVoxelMap<KeyType, ValueType, Allocator>& MapA,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapB,
		const IterateType Iterate)
	{
		FVoxelUtilities::IterateSortedMaps(
			MapA,
			MapB,
			TLess<KeyType>(),
			Iterate);
	}
}