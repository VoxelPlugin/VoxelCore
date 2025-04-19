// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"
#include "VoxelMinimal/Containers/VoxelSet.h"
#include "VoxelMinimal/Containers/VoxelMap.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Containers/VoxelSparseArray.h"
#include "VoxelMinimal/Containers/VoxelChunkedArray.h"
#include "VoxelMinimal/Containers/VoxelChunkedSparseArray.h"

// Voxel::ParallelFor is 15% faster on simple test cases
// It won't handle highly unbalanced flows though
#define VOXEL_USE_STOCK_PARALLEL_FOR 0

#if VOXEL_USE_STOCK_PARALLEL_FOR
#include "Async/ParallelFor.h"
#endif

namespace Voxel
{
	namespace Internal
	{
		VOXELCORE_API int32 GetMaxNumThreads();

		VOXELCORE_API void ParallelFor(
			int64 Num,
			TFunctionRef<void(int64 StartIndex, int64 EndIndex)> Lambda);
	}

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	template<typename SizeType, typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(SizeType)>
	void ParallelFor(
		const SizeType Num,
		LambdaType Lambda)
	{
#if VOXEL_USE_STOCK_PARALLEL_FOR
		::ParallelFor(Num, Lambda);
#else
		Internal::ParallelFor(Num, [&](const int64 StartIndex, const int64 EndIndex)
		{
			for (int64 Index = StartIndex; Index < EndIndex; Index++)
			{
				Lambda(Index);
			}
		});
#endif
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename Type,
		typename SizeType,
		typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)> ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type)>) ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type, SizeType)>)
	)
	void ParallelFor(
		const TVoxelArrayView<Type, SizeType> ArrayView,
		LambdaType Lambda)
	{
		Internal::ParallelFor(ArrayView.Num(), [&](const SizeType StartIndex, const SizeType EndIndex)
		{
			if constexpr (
				LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
				LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)>)
			{
				Lambda(ArrayView.Slice(StartIndex, EndIndex - StartIndex));
			}
			else if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type&)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&)> ||
				(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type)>))
			{
				for (SizeType Index = StartIndex; Index < EndIndex; Index++)
				{
					Lambda(ArrayView[Index]);
				}
			}
			else if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)> ||
				(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type, SizeType)>))
			{
				for (SizeType Index = StartIndex; Index < EndIndex; Index++)
				{
					Lambda(ArrayView[Index], Index);
				}
			}
			else
			{
				checkStatic(std::is_same_v<LambdaType, void>);
			}
		});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename Type,
		typename Allocator,
		typename LambdaType,
		typename SizeType = typename Allocator::SizeType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(Type&, SizeType)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)> ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type)>) ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type, SizeType)>)
	)
	void ParallelFor(
		TArray<Type, Allocator>& Array,
		LambdaType Lambda)
	{
		Voxel::ParallelFor(
			MakeVoxelArrayView(Array),
			MoveTemp(Lambda));
	}

	template<
		typename Type,
		typename Allocator,
		typename LambdaType,
		typename SizeType = typename Allocator::SizeType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(TVoxelArrayView<const Type, SizeType>)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		LambdaHasSignature_V<LambdaType, void(const Type&, SizeType)> ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type)>) ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type, SizeType)>)
	)
	void ParallelFor(
		const TArray<Type, Allocator>& Array,
		LambdaType Lambda)
	{
		Voxel::ParallelFor(
			MakeVoxelArrayView(Array),
			MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename Type, typename Allocator, typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(const Type&)> ||
		(std::is_trivially_destructible_v<Type> && LambdaHasSignature_V<LambdaType, void(Type)>)
	)
	void ParallelFor(
		TVoxelSet<Type, Allocator>& Set,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Set.GetElements(),
			[&](const typename TVoxelSet<Type, Allocator>::FElement& Element)
			{
				Lambda(Element.Value);
			});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename KeyType, typename ValueType, typename Allocator, typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(typename TVoxelMap<KeyType, ValueType, Allocator>::FElement&)> ||
		LambdaHasSignature_V<LambdaType, void(const typename TVoxelMap<KeyType, ValueType, Allocator>::FElement&)>
	)
	void ParallelFor(
		TVoxelMap<KeyType, ValueType, Allocator>& Map,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Map.GetElements(),
			MoveTemp(Lambda));
	}

	template<typename KeyType, typename ValueType, typename Allocator, typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(const typename TVoxelMap<KeyType, ValueType, Allocator>::FElement&)>
	void ParallelFor(
		const TVoxelMap<KeyType, ValueType, Allocator>& Map,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Map.GetElements(),
			MoveTemp(Lambda));
	}
	template<typename KeyType, typename ValueType, typename Allocator, typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(const KeyType&)>
	void ParallelFor_Keys(
		const TVoxelMap<KeyType, ValueType, Allocator>& Map,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Map.GetElements(),
			[&](const typename TVoxelMap<KeyType, ValueType, Allocator>::FElement& Element)
			{
				Lambda(Element.Key);
			});
	}

	template<typename KeyType, typename ValueType, typename Allocator, typename LambdaType>
	requires
	(
		LambdaHasSignature_V<LambdaType, void(ValueType&)> ||
		LambdaHasSignature_V<LambdaType, void(const ValueType&)>
	)
	void ParallelFor_Values(
		TVoxelMap<KeyType, ValueType, Allocator>& Map,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Map.GetElements(),
			[&](typename TVoxelMap<KeyType, ValueType, Allocator>::FElement& Element)
			{
				Lambda(Element.Value);
			});
	}

	template<typename KeyType, typename ValueType, typename Allocator, typename LambdaType>
	requires LambdaHasSignature_V<LambdaType, void(const ValueType&)>
	void ParallelFor_Values(
		const TVoxelMap<KeyType, ValueType, Allocator>& Map,
		LambdaType Lambda)
	{
		return Voxel::ParallelFor(
			Map.GetElements(),
			[&](const typename TVoxelMap<KeyType, ValueType, Allocator>::FElement& Element)
			{
				Lambda(Element.Value);
			});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename Type,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(Type&)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		TVoxelSparseArray<Type>& Array,
		LambdaType Lambda)
	{
		Internal::ParallelFor(Array.Max_Unsafe(), [&](const int32 StartIndex, const int32 EndIndex)
		{
			if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type)> ||
				LambdaHasSignature_V<LambdaType, void(Type&)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					if (!Array.IsAllocated(Index))
					{
						continue;
					}

					Lambda(Array[Index]);
				}
			}
			else if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type, int32)> ||
				LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&, int32)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					if (!Array.IsAllocated(Index))
					{
						continue;
					}

					Lambda(Array[Index], Index);
				}
			}
			else
			{
				checkStatic(std::is_same_v<LambdaType, void>);
			}
		});
	}

	template<
		typename Type,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		const TVoxelSparseArray<Type>& Array,
		LambdaType Lambda)
	{
		Voxel::ParallelFor(ConstCast(Array), MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename Type,
		int32 MaxBytesPerChunk,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(Type&)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		TVoxelChunkedArray<Type, MaxBytesPerChunk>& Array,
		LambdaType Lambda)
	{
		Internal::ParallelFor(Array.Num(), [&](const int32 StartIndex, const int32 EndIndex)
		{
			if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type)> ||
				LambdaHasSignature_V<LambdaType, void(Type&)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					Lambda(Array[Index]);
				}
			}
			else if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type, int32)> ||
				LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&, int32)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					Lambda(Array[Index], Index);
				}
			}
			else
			{
				checkStatic(std::is_same_v<LambdaType, void>);
			}
		});
	}

	template<
		typename Type,
		int32 MaxBytesPerChunk,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		const TVoxelChunkedArray<Type, MaxBytesPerChunk>& Array,
		LambdaType Lambda)
	{
		Voxel::ParallelFor(ConstCast(Array), MoveTemp(Lambda));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<
		typename Type,
		int32 NumPerChunk,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(Type&)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		TVoxelChunkedSparseArray<Type, NumPerChunk>& Array,
		LambdaType Lambda)
	{
		Internal::ParallelFor(Array.Max_Unsafe(), [&](const int32 StartIndex, const int32 EndIndex)
		{
			if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type)> ||
				LambdaHasSignature_V<LambdaType, void(Type&)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					if (!Array.IsAllocated_ValidIndex(Index))
					{
						continue;
					}

					Lambda(Array[Index]);
				}
			}
			else if constexpr (
				LambdaHasSignature_V<LambdaType, void(Type, int32)> ||
				LambdaHasSignature_V<LambdaType, void(Type&, int32)> ||
				LambdaHasSignature_V<LambdaType, void(const Type&, int32)>)
			{
				for (int32 Index = StartIndex; Index < EndIndex; Index++)
				{
					if (!Array.IsAllocated_ValidIndex(Index))
					{
						continue;
					}

					Lambda(Array[Index], Index);
				}
			}
			else
			{
				checkStatic(std::is_same_v<LambdaType, void>);
			}
		});
	}

	template<
		typename Type,
		int32 NumPerChunk,
		typename LambdaType,
		typename = std::enable_if_t<
			LambdaHasSignature_V<LambdaType, void(const Type&)> ||
			LambdaHasSignature_V<LambdaType, void(Type)> ||
			LambdaHasSignature_V<LambdaType, void(const Type&, int32)> ||
			LambdaHasSignature_V<LambdaType, void(Type, int32)>>>
	void ParallelFor(
		const TVoxelChunkedSparseArray<Type, NumPerChunk>& Array,
		LambdaType Lambda)
	{
		Voxel::ParallelFor(ConstCast(Array), MoveTemp(Lambda));
	}
}