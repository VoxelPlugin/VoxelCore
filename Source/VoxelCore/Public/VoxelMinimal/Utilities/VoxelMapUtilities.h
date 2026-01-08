// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelIterate.h"
#include "VoxelMinimal/Containers/VoxelMap.h"

namespace FVoxelUtilities
{
	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename PredicateType,
		typename IterateType>
	requires
	(
		LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*)> ||
		LambdaHasSignature_V<IterateType, EVoxelIterate(const KeyType&, const ValueType*, const ValueType*)>
	)

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
			IndexA < ElementsA.Num() ||
			IndexB < ElementsB.Num())
		{
			const FElement* A = IndexA < ElementsA.Num() ? &ElementsA[IndexA] : nullptr;
			const FElement* B = IndexB < ElementsB.Num() ? &ElementsB[IndexB] : nullptr;

			const KeyType* Key;
			const ValueType* ValueA;
			const ValueType* ValueB;

			if (A && (!B || Less(A->Key, B->Key)))
			{
				Key = &A->Key;
				ValueA = &A->Value;
				ValueB = nullptr;
				IndexA++;
			}
			else if (B && (!A || Less(B->Key, A->Key)))
			{
				Key = &B->Key;
				ValueA = nullptr;
				ValueB = &B->Value;
				IndexB++;
			}
			else
			{
				checkVoxelSlow(A->Key == B->Key);

				Key = &A->Key;
				ValueA = &A->Value;
				ValueB = &B->Value;
				IndexA++;
				IndexB++;
			}

			if constexpr (std::is_void_v<LambdaReturnType_T<IterateType>>)
			{
				Iterate(*Key, ValueA, ValueB);
			}
			else
			{
				if (Iterate(*Key, ValueA, ValueB) == EVoxelIterate::Stop)
				{
					return;
				}
			}
		}

		checkVoxelSlow(IndexA == ElementsA.Num());
		checkVoxelSlow(IndexB == ElementsB.Num());
	}

	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename PredicateType,
		typename IterateType>
	requires
	(
		LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*, const ValueType*)> ||
		LambdaHasSignature_V<IterateType, EVoxelIterate(const KeyType&, const ValueType*, const ValueType*, const ValueType*)>
	)
	void IterateSortedMaps(
		const TVoxelMap<KeyType, ValueType, Allocator>& MapA,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapB,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapC,
		const PredicateType Less,
		const IterateType Iterate)
	{
		VOXEL_FUNCTION_COUNTER_NUM(FMath::Max3(MapA.Num(), MapB.Num(), MapC.Num()), 128);
		checkVoxelSlow(MapA.AreKeySorted(Less));
		checkVoxelSlow(MapB.AreKeySorted(Less));
		checkVoxelSlow(MapC.AreKeySorted(Less));

		using FElement = TVoxelMapElement<KeyType, ValueType>;

		const TConstVoxelArrayView<FElement> ElementsA = MapA.GetElements();
		const TConstVoxelArrayView<FElement> ElementsB = MapB.GetElements();
		const TConstVoxelArrayView<FElement> ElementsC = MapC.GetElements();

		int32 IndexA = 0;
		int32 IndexB = 0;
		int32 IndexC = 0;

		while (
			IndexA < ElementsA.Num() ||
			IndexB < ElementsB.Num() ||
			IndexC < ElementsC.Num())
		{
			const FElement* A = IndexA < ElementsA.Num() ? &ElementsA[IndexA] : nullptr;
			const FElement* B = IndexB < ElementsB.Num() ? &ElementsB[IndexB] : nullptr;
			const FElement* C = IndexC < ElementsC.Num() ? &ElementsC[IndexC] : nullptr;

			// Find the minimum key among available elements
			const KeyType* MinKey = nullptr;
			if (A)
			{
				MinKey = &A->Key;
			}
			if (B && (!MinKey || Less(B->Key, *MinKey)))
			{
				MinKey = &B->Key;
			}
			if (C && (!MinKey || Less(C->Key, *MinKey)))
			{
				MinKey = &C->Key;
			}

			checkVoxelSlow(MinKey);

			// Collect values for this key
			const ValueType* ValueA = (A && A->Key == *MinKey) ? &A->Value : nullptr;
			const ValueType* ValueB = (B && B->Key == *MinKey) ? &B->Value : nullptr;
			const ValueType* ValueC = (C && C->Key == *MinKey) ? &C->Value : nullptr;

			if constexpr (std::is_void_v<LambdaReturnType_T<IterateType>>)
			{
				Iterate(*MinKey, ValueA, ValueB, ValueC);
			}
			else
			{
				if (Iterate(*MinKey, ValueA, ValueB, ValueC) == EVoxelIterate::Stop)
				{
					return;
				}
			}

			// Advance indices for maps that had this key
			if (ValueA)
			{
				IndexA++;
			}
			if (ValueB)
			{
				IndexB++;
			}
			if (ValueC)
			{
				IndexC++;
			}
		}

		checkVoxelSlow(IndexA == ElementsA.Num());
		checkVoxelSlow(IndexB == ElementsB.Num());
		checkVoxelSlow(IndexC == ElementsC.Num());
	}

	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename IterateType>
	requires
	(
		LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*)> ||
		LambdaHasSignature_V<IterateType, EVoxelIterate(const KeyType&, const ValueType*, const ValueType*)>
	)
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

	template<
		typename KeyType,
		typename ValueType,
		typename Allocator,
		typename IterateType>
	requires
	(
		LambdaHasSignature_V<IterateType, void(const KeyType&, const ValueType*, const ValueType*, const ValueType*)> ||
		LambdaHasSignature_V<IterateType, EVoxelIterate(const KeyType&, const ValueType*, const ValueType*, const ValueType*)>
	)
	void IterateSortedMaps(
		const TVoxelMap<KeyType, ValueType, Allocator>& MapA,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapB,
		const TVoxelMap<KeyType, ValueType, Allocator>& MapC,
		const IterateType Iterate)
	{
		FVoxelUtilities::IterateSortedMaps(
			MapA,
			MapB,
			MapC,
			TLess<KeyType>(),
			Iterate);
	}
}