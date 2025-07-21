// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Algo/IsSorted.h"
#include "Compression/OodleDataCompression.h"
#include "VoxelMinimal/VoxelOptional.h"
#include "VoxelMinimal/VoxelOctahedron.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"

namespace FVoxelUtilities
{
	FORCEINLINE bool MemoryEqual(const void* Buf1, const void* Buf2, const SIZE_T Count)
	{
		return FPlatformMemory::Memcmp(Buf1, Buf2, Count) == 0;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType>
	using ElementType = std::remove_reference_t<decltype(*GetData(std::declval<ArrayType>()))>;

	template<typename ArrayType>
	const bool IsMutableArray = !std::is_const_v<ElementType<ArrayType>>;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		(
			std::is_trivially_destructible_v<ElementType<ArrayType>> ||
			bool(TIsZeroConstructType<ElementType<ArrayType>>::Value)
		)
	)
	FORCEINLINE void Memzero(ArrayType&& Array)
	{
		FMemory::Memzero(GetData(Array), GetNum(Array) * sizeof(ElementType<ArrayType>));
	}
	template<typename ArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		std::is_trivially_destructible_v<ElementType<ArrayType>>
	)
	FORCEINLINE void Memset(ArrayType&& Array, const uint8 Value)
	{
		FMemory::Memset(GetData(Array), Value, GetNum(Array) * sizeof(ElementType<ArrayType>));
	}

	template<typename DstArrayType, typename SrcArrayType>
	requires
	(
		IsMutableArray<DstArrayType> &&
		std::is_trivially_destructible_v<ElementType<DstArrayType>> &&
		std::is_same_v<ElementType<DstArrayType>, std::remove_const_t<ElementType<SrcArrayType>>>
	)
	FORCEINLINE void Memcpy(DstArrayType&& Dest, SrcArrayType&& Src)
	{
		checkVoxelSlow(GetNum(Dest) == GetNum(Src));
		FMemory::Memcpy(GetData(Dest), GetData(Src), GetNum(Dest) * sizeof(ElementType<DstArrayType>));
	}

	template<typename ArrayTypeA, typename ArrayTypeB>
	requires
	(
		std::is_same_v<std::remove_const_t<ElementType<ArrayTypeA>>, std::remove_const_t<ElementType<ArrayTypeB>>> &&
		std::is_trivially_destructible_v<ElementType<ArrayTypeA>>
	)
	FORCEINLINE bool Equal(ArrayTypeA&& A, ArrayTypeB&& B)
	{
		return
			GetNum(A) == GetNum(B) &&
			FVoxelUtilities::MemoryEqual(GetData(A), GetData(B), GetNum(A) * sizeof(ElementType<ArrayTypeA>));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		(
			std::is_trivially_destructible_v<ElementType<ArrayType>> ||
			bool(TIsZeroConstructType<ElementType<ArrayType>>::Value)
		)
	)
	void Memzero_Stats(ArrayType&& Array)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Array), 4096);
		FVoxelUtilities::Memzero(Array);
	}
	template<typename ArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		std::is_trivially_destructible_v<ElementType<ArrayType>>
	)
	void Memset_Stats(ArrayType&& Array, const uint8 Value)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Array), 4096);
		FVoxelUtilities::Memset(Array, Value);
	}

	template<typename DstArrayType, typename SrcArrayType>
	requires
	(
		IsMutableArray<DstArrayType> &&
		std::is_trivially_destructible_v<ElementType<DstArrayType>> &&
		std::is_same_v<ElementType<DstArrayType>, std::remove_const_t<ElementType<SrcArrayType>>>
	)
	FORCEINLINE void Memcpy_Stats(DstArrayType&& Dest, SrcArrayType&& Src)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Dest), 4096);
		FVoxelUtilities::Memcpy(Dest, Src);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType, typename NumType>
	FORCENOINLINE void SetNum(ArrayType& Array, NumType Num)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num, 1024);

		Array.Reserve(Num);
		Array.SetNum(Num);
	}

	template<typename ArrayType, typename NumType>
	requires std::is_trivially_destructible_v<ElementType<ArrayType>>
	FORCENOINLINE void SetNumFast(ArrayType& Array, NumType Num)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num, 4096);

		Array.Reserve(Num);
		Array.SetNumUninitialized(Num, EAllowShrinking::No);
	}

	template<typename ArrayType, typename NumType>
	requires
	(
		std::is_trivially_destructible_v<ElementType<ArrayType>> ||
		bool(TIsZeroConstructType<ElementType<ArrayType>>::Value)
	)
	FORCENOINLINE void SetNumZeroed(ArrayType& Array, NumType Num)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num, 1024);

		Array.Reserve(Num);
		Array.SetNumZeroed(Num);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T, typename ArrayType>
	requires std::is_same_v<ElementType<ArrayType>, uint8>
	FORCEINLINE T& CastBytes(ArrayType&& Array)
	{
		checkVoxelSlow(GetNum(Array) == sizeof(T));
		return *reinterpret_cast<T*>(GetData(Array));
	}
	template<typename T, typename ArrayType>
	requires std::is_same_v<ElementType<ArrayType>, const uint8>
	FORCEINLINE const T& CastBytes(ArrayType&& Array)
	{
		checkVoxelSlow(GetNum(Array) == sizeof(T));
		return *reinterpret_cast<const T*>(GetData(Array));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		std::is_trivially_destructible_v<ElementType<ArrayType>>
	)
	FORCENOINLINE void SetAll(ArrayType&& Array, const ElementType<ArrayType> Value)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Array), 1024);

		ElementType<ArrayType>* RESTRICT Start = GetData(Array);
		const ElementType<ArrayType>* RESTRICT End = Start + GetNum(Array);
		while (Start != End)
		{
			*Start = Value;
			Start++;
		}
	}

	template<typename ArrayType>
	requires std::is_trivially_destructible_v<ElementType<ArrayType>>
	void ForceBulkSerializeArray(FArchive& Ar, ArrayType& Array)
	{
		if (Ar.IsLoading())
		{
			typename ArrayType::SizeType NewArrayNum = 0;
			Ar << NewArrayNum;
			Array.Empty(NewArrayNum);
			Array.AddUninitialized(NewArrayNum);
			Ar.Serialize(Array.GetData(), NewArrayNum * Array.GetTypeSize());
		}
		else if (Ar.IsSaving())
		{
			typename ArrayType::SizeType ArrayCount = Array.Num();
			Ar << ArrayCount;
			Ar.Serialize(Array.GetData(), ArrayCount * Array.GetTypeSize());
		}
	}

	template<typename ArrayType, typename IndicesArrayType>
	requires
	(
		IsMutableArray<ArrayType> &&
		std::is_trivially_destructible_v<ElementType<ArrayType>>
	)
	void RemoveAt(ArrayType& Array, const IndicesArrayType& SortedIndicesToRemove)
	{
		if (SortedIndicesToRemove.Num() == 0)
		{
			return;
		}

#if VOXEL_DEBUG
		for (int64 Index = 0; Index < SortedIndicesToRemove.Num() - 1; Index++)
		{
			check(SortedIndicesToRemove[Index] < SortedIndicesToRemove[Index + 1]);
			check(Array.IsValidIndex(SortedIndicesToRemove[Index]));
			check(Array.IsValidIndex(SortedIndicesToRemove[Index + 1]));
		}
#endif

		if (SortedIndicesToRemove.Num() == Array.Num())
		{
			Array.Reset();
			return;
		}

		for (int64 Index = 0; Index < SortedIndicesToRemove.Num() - 1; Index++)
		{
			const int64 IndexToRemove = SortedIndicesToRemove[Index];
			const int64 NextIndexToRemove = SortedIndicesToRemove[Index + 1];

			// -1: don't count the element we're removing
			const int64 Count = NextIndexToRemove - IndexToRemove - 1;
			checkVoxelSlow(Count >= 0);

			if (Count == 0)
			{
				continue;
			}

			FMemory::Memmove(
				&Array[IndexToRemove - Index],
				&Array[IndexToRemove + 1],
				Count * sizeof(ElementType<ArrayType>));
		}

		{
			const int64 Index = SortedIndicesToRemove.Num() - 1;
			const int64 IndexToRemove = SortedIndicesToRemove[Index];
			const int64 NextIndexToRemove = Array.Num();

			// -1: don't count the element we're removing
			const int64 Count = NextIndexToRemove - IndexToRemove - 1;
			checkVoxelSlow(Count >= 0);

			if (Count != 0)
			{
				FMemory::Memmove(
					&Array[IndexToRemove - Index],
					&Array[IndexToRemove + 1],
					Count * sizeof(ElementType<ArrayType>));
			}
		}

		Array.SetNum(Array.Num() - SortedIndicesToRemove.Num(), false);
	}

	template<
		typename ArrayTypeA,
		typename ArrayTypeB,
		typename OutArrayTypeA,
		typename OutArrayTypeB,
		typename PredicateType>
		void DiffSortedArrays(
		const ArrayTypeA& ArrayA,
		const ArrayTypeB& ArrayB,
		OutArrayTypeA& OutOnlyInA,
		OutArrayTypeB& OutOnlyInB,
		PredicateType Less)
	{
		VOXEL_FUNCTION_COUNTER_NUM(FMath::Max(ArrayA.Num(), ArrayB.Num()), 128);

		checkVoxelSlow(Algo::IsSorted(ArrayA, Less));
		checkVoxelSlow(Algo::IsSorted(ArrayB, Less));

		int64 IndexA = 0;
		int64 IndexB = 0;
		while (
			IndexA < ArrayA.Num() &&
			IndexB < ArrayB.Num())
		{
			const auto& A = ArrayA[IndexA];
			const auto& B = ArrayB[IndexB];

			if (Less(A, B))
			{
				OutOnlyInA.Add(A);
				IndexA++;
			}
			else if (Less(B, A))
			{
				OutOnlyInB.Add(B);
				IndexB++;
			}
			else
			{
				IndexA++;
				IndexB++;
			}
		}

		while (IndexA < ArrayA.Num())
		{
			OutOnlyInA.Add(ArrayA[IndexA]);
			IndexA++;
		}

		while (IndexB < ArrayB.Num())
		{
			OutOnlyInB.Add(ArrayB[IndexB]);
			IndexB++;
		}

		checkVoxelSlow(IndexA == ArrayA.Num());
		checkVoxelSlow(IndexB == ArrayB.Num());
	}

	template<typename KeyType, typename ValueType, typename ArrayType>
	void ReorderMapKeys(
		TMap<KeyType, ValueType>& KeyToValue,
		const ArrayType& NewKeys)
	{
		ensure(KeyToValue.Num() == NewKeys.Num());

		TMap<KeyType, int32> KeyToIndex;
		KeyToIndex.Reserve(NewKeys.Num());
		for (const KeyType& Guid : NewKeys)
		{
			ensure(!KeyToIndex.Contains(Guid));
			KeyToIndex.Add(Guid, KeyToIndex.Num());
		}

		KeyToValue.KeyStableSort([&](const KeyType& KeyA, const KeyType& KeyB)
		{
			ensure(KeyToIndex.Contains(KeyA));
			ensure(KeyToIndex.Contains(KeyB));

			return KeyToIndex.FindRef(KeyA) < KeyToIndex.FindRef(KeyB);
		});
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE T Get2DIndex(const int32 SizeX, const int32 SizeY, const int32 X, const int32 Y)
	{
		checkVoxelSlow(0 <= X && X < SizeX);
		checkVoxelSlow(0 <= Y && Y < SizeY);
		checkVoxelSlow(int64(SizeX) * int64(SizeY) <= TNumericLimits<T>::Max());
		return T(X) + T(Y) * SizeX;
	}
	template<typename T>
	FORCEINLINE T Get2DIndex(const int32 Size, const int32 X, const int32 Y)
	{
		return Get2DIndex<T>(Size, Size, X, Y);
	}
	template<typename T>
	FORCEINLINE T Get2DIndex(const FIntPoint& Size, const int32 X, const int32 Y)
	{
		return Get2DIndex<T>(Size.X, Size.Y, X, Y);
	}
	template<typename T>
	FORCEINLINE T Get2DIndex(const int32 Size, const FIntPoint& Position)
	{
		return Get2DIndex<T>(Size, Position.X, Position.Y);
	}
	template<typename T>
	FORCEINLINE T Get2DIndex(const FIntPoint& Size, const FIntPoint& Position)
	{
		return Get2DIndex<T>(Size, Position.X, Position.Y);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE T Get3DIndex(const FIntVector& Size, const int32 X, const int32 Y, const int32 Z)
	{
		checkVoxelSlow(0 <= X && X < Size.X);
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);
		checkVoxelSlow(int64(Size.X) * int64(Size.Y) * int64(Size.Z) <= TNumericLimits<T>::Max());
		return T(X) + T(Y) * Size.X + T(Z) * Size.X * Size.Y;
	}
	template<typename T>
	FORCEINLINE T Get3DIndex(const int32 Size, const int32 X, const int32 Y, const int32 Z)
	{
		return Get3DIndex<T>(FIntVector(Size), X, Y, Z);
	}
	template<typename T>
	FORCEINLINE T Get3DIndex(const FIntVector& Size, const FIntVector& Position)
	{
		return Get3DIndex<T>(Size, Position.X, Position.Y, Position.Z);
	}
	template<typename T>
	FORCEINLINE T Get3DIndex(const int32 Size, const FIntVector& Position)
	{
		return Get3DIndex<T>(FIntVector(Size), Position);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE FIntPoint Break2DIndex_Log2(const int32 SizeLog2, const T Index)
	{
		const int32 X = (Index >> (0 * SizeLog2)) & ((1 << SizeLog2) - 1);
		const int32 Y = (Index >> (1 * SizeLog2)) & ((1 << SizeLog2) - 1);
		checkVoxelSlow(Get2DIndex<T>(1 << SizeLog2, X, Y) == Index);
		return { X, Y };
	}
	template<typename T>
	FORCEINLINE FIntPoint Break2DIndex(const FIntPoint& Size, T Index)
	{
		const T OriginalIndex = Index;

		const int32 Y = Index / Size.X;
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		Index -= Y * Size.X;

		const int32 X = Index;
		checkVoxelSlow(0 <= X && X < Size.X);

		checkVoxelSlow(Get2DIndex<T>(Size, X, Y) == OriginalIndex);

		return { X, Y };
	}
	template<typename T>
	FORCEINLINE FIntPoint Break2DIndex(const int32 Size, const T Index)
	{
		return FVoxelUtilities::Break2DIndex<T>(FIntPoint(Size), Index);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE FIntVector Break3DIndex_Log2(const int32 SizeLog2, const T Index)
	{
		const int32 X = (Index >> (0 * SizeLog2)) & ((1 << SizeLog2) - 1);
		const int32 Y = (Index >> (1 * SizeLog2)) & ((1 << SizeLog2) - 1);
		const int32 Z = (Index >> (2 * SizeLog2)) & ((1 << SizeLog2) - 1);
		checkVoxelSlow(Get3DIndex<T>(1 << SizeLog2, X, Y, Z) == Index);
		return { X, Y, Z };
	}
	template<typename T>
	FORCEINLINE FIntVector Break3DIndex(const FIntVector& Size, T Index)
	{
		const T OriginalIndex = Index;

		const int32 Z = Index / (Size.X * Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);
		Index -= Z * Size.X * Size.Y;

		const int32 Y = Index / Size.X;
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		Index -= Y * Size.X;

		const int32 X = Index;
		checkVoxelSlow(0 <= X && X < Size.X);

		checkVoxelSlow(Get3DIndex<T>(Size, X, Y, Z) == OriginalIndex);

		return { X, Y, Z };
	}
	template<typename T>
	FORCEINLINE FIntVector Break3DIndex(const int32 Size, const T Index)
	{
		return FVoxelUtilities::Break3DIndex<T>(FIntVector(Size), Index);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API void Memcpy_Convert(
		TVoxelArrayView<double> Dest,
		TConstVoxelArrayView<float> Src);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API bool AllEqual(TConstVoxelArrayView<uint8> Data, uint8 Value);
	VOXELCORE_API bool AllEqual(TConstVoxelArrayView<uint16> Data, uint16 Value);
	VOXELCORE_API bool AllEqual(TConstVoxelArrayView<uint32> Data, uint32 Value);
	VOXELCORE_API bool AllEqual(TConstVoxelArrayView<uint64> Data, uint64 Value);

	VOXELCORE_API uint16 GetMin(TConstVoxelArrayView<uint16> Data);
	VOXELCORE_API uint16 GetMax(TConstVoxelArrayView<uint16> Data);

	VOXELCORE_API float GetMin(TConstVoxelArrayView<float> Data);
	VOXELCORE_API float GetMax(TConstVoxelArrayView<float> Data);

	VOXELCORE_API float GetAbsMin(TConstVoxelArrayView<float> Data);
	VOXELCORE_API float GetAbsMax(TConstVoxelArrayView<float> Data);

	// Will skip NaNs and infinite values
	VOXELCORE_API float GetAbsMinSafe(TConstVoxelArrayView<float> Data);
	VOXELCORE_API float GetAbsMaxSafe(TConstVoxelArrayView<float> Data);

	VOXELCORE_API FInt32Interval GetMinMax(TConstVoxelArrayView<uint8> Data);
	VOXELCORE_API FInt32Interval GetMinMax(TConstVoxelArrayView<uint16> Data);
	VOXELCORE_API FInt32Interval GetMinMax(TConstVoxelArrayView<int32> Data);
	VOXELCORE_API TInterval<int64> GetMinMax(TConstVoxelArrayView<int64> Data);
	// Will return { 0, 0 } if no values are valid
	// Won't check for NaNs
	VOXELCORE_API FFloatInterval GetMinMax(TConstVoxelArrayView<float> Data);
	VOXELCORE_API FDoubleInterval GetMinMax(TConstVoxelArrayView<double> Data);

	VOXELCORE_API void GetMinMax(
		TConstVoxelArrayView<FVector2f> Data,
		FVector2f& OutMin,
		FVector2f& OutMax);

	VOXELCORE_API void GetMinMax(
		TConstVoxelArrayView<FColor> Data,
		FColor& OutMin,
		FColor& OutMax);

	// Will return { MAX_flt, -MAX_flt } if no values are valid
	// Will skip NaNs and infinite values
	VOXELCORE_API FFloatInterval GetMinMaxSafe(TConstVoxelArrayView<float> Data);
	// Will return { MAX_dbl, -MAX_dbl } if no values are valid
	// Will skip NaNs and infinite values
	VOXELCORE_API FDoubleInterval GetMinMaxSafe(TConstVoxelArrayView<double> Data);

	// Will re-normalize Vectors
	VOXELCORE_API TVoxelArray<FVoxelOctahedron> MakeOctahedrons(TConstVoxelArrayView<FVector3f> Vectors);

	// Will re-normalize Vectors
	VOXELCORE_API TVoxelArray<FVoxelOctahedron> MakeOctahedrons(
		TConstVoxelArrayView<float> X,
		TConstVoxelArrayView<float> Y,
		TConstVoxelArrayView<float> Z);

	// Will replace -0 by +0
	VOXELCORE_API void FixupSignBit(TVoxelArrayView<float> Data);

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API bool IsCompressedData(TConstVoxelArrayView64<uint8> CompressedData);

	VOXELCORE_API TVoxelArray64<uint8> Compress(
		TConstVoxelArrayView64<uint8> Data,
		bool bAllowParallel = true,
		FOodleDataCompression::ECompressor Compressor = FOodleDataCompression::ECompressor::Leviathan,
		FOodleDataCompression::ECompressionLevel CompressionLevel = FOodleDataCompression::ECompressionLevel::Optimal3);

	VOXELCORE_API bool Decompress(
		TConstVoxelArrayView64<uint8> CompressedData,
		TVoxelArray64<uint8>& OutData,
		bool bAllowParallel = true);
}