// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Algo/IsSorted.h"

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
	FORCEINLINE void Memzero(ArrayType&& Array)
	{
		FMemory::Memzero(GetData(Array), GetNum(Array) * sizeof(decltype(*GetData(Array))));
	}
	template<typename ArrayType>
	FORCEINLINE void Memset(ArrayType&& Array, const uint8 Value)
	{
		FMemory::Memset(GetData(Array), Value, GetNum(Array) * sizeof(decltype(*GetData(Array))));
	}

	template<typename DstArrayType, typename SrcArrayType, typename = typename TEnableIf<
		!TIsConst<typename TRemoveReference<decltype(*GetData(DeclVal<DstArrayType>()))>::Type>::Value &&
		std::is_same_v<
			VOXEL_GET_TYPE(*GetData(DeclVal<DstArrayType>())),
			VOXEL_GET_TYPE(*GetData(DeclVal<SrcArrayType>()))
		>>::Type>
	FORCEINLINE void Memcpy(DstArrayType&& Dest, SrcArrayType&& Src)
	{
		checkVoxelSlow(GetNum(Dest) == GetNum(Src));
		FMemory::Memcpy(GetData(Dest), GetData(Src), GetNum(Dest) * sizeof(decltype(*GetData(Dest))));
	}

	template<typename ArrayTypeA, typename ArrayTypeB, typename = typename TEnableIf<
		std::is_same_v<
			VOXEL_GET_TYPE(*GetData(DeclVal<ArrayTypeA>())),
			VOXEL_GET_TYPE(*GetData(DeclVal<ArrayTypeB>()))
		> &&
		TIsTriviallyDestructible<VOXEL_GET_TYPE(*GetData(DeclVal<ArrayTypeA>()))>::Value
	>::Type>
	FORCEINLINE bool MemoryEqual(ArrayTypeA&& A, ArrayTypeB&& B)
	{
		checkVoxelSlow(GetNum(A) == GetNum(B));
		return FPlatformMemory::Memcmp(GetData(A), GetData(B), GetNum(A) * sizeof(decltype(*GetData(A)))) == 0;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType>
	void LargeMemzero(ArrayType&& Array)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Array), 4096);
		FVoxelUtilities::Memzero(Array);
	}
	template<typename ArrayType>
	void LargeMemset(ArrayType&& Array, const uint8 Value)
	{
		VOXEL_FUNCTION_COUNTER_NUM(GetNum(Array), 4096);
		FVoxelUtilities::Memset(Array, Value);
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
	template<typename ArrayType>
	FORCEINLINE void SetNum(ArrayType& Array, const FIntVector& Size)
	{
		FVoxelUtilities::SetNum(Array, int64(Size.X) * int64(Size.Y) * int64(Size.Z));
	}

	template<typename ArrayType, typename NumType, typename = typename TEnableIf<TIsTriviallyDestructible<VOXEL_GET_TYPE(*::GetData(DeclVal<ArrayType>()))>::Value>::Type>
	FORCENOINLINE void SetNumFast(ArrayType& Array, NumType Num)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num, 4096);

		Array.Reserve(Num);
		Array.SetNumUninitialized(Num, false);
	}
	template<typename ArrayType, typename = typename TEnableIf<TIsTriviallyDestructible<VOXEL_GET_TYPE(*::GetData(DeclVal<ArrayType>()))>::Value>::Type>
	FORCEINLINE void SetNumFast(ArrayType& Array, const FIntVector& Size)
	{
		FVoxelUtilities::SetNumFast(Array, int64(Size.X) * int64(Size.Y) * int64(Size.Z));
	}

	template<typename ArrayType, typename NumType>
	FORCENOINLINE void SetNumZeroed(ArrayType& Array, NumType Num)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num, 1024);

		Array.Reserve(Num);
		Array.SetNumZeroed(Num);
	}
	template<typename ArrayType>
	FORCEINLINE void SetNumZeroed(ArrayType& Array, const FIntVector& Size)
	{
		FVoxelUtilities::SetNumZeroed(Array, int64(Size.X) * int64(Size.Y) * int64(Size.Z));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename ArrayType, typename = typename TEnableIf<!TIsConst<typename TRemoveReference<decltype(*GetData(DeclVal<ArrayType>()))>::Type>::Value>::Type>
	FORCENOINLINE void SetAll(ArrayType&& Array, const VOXEL_GET_TYPE(*GetData(DeclVal<ArrayType>())) Value)
	{
		using T = VOXEL_GET_TYPE(*GetData(DeclVal<ArrayType>()));

		T* RESTRICT Start = GetData(Array);
		const T* RESTRICT End = Start + GetNum(Array);
		while (Start != End)
		{
			*Start = Value;
			Start++;
		}
	}

	template<typename ArrayType>
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
	void RemoveAt(ArrayType& Array, const IndicesArrayType& SortedIndicesToRemove)
	{
		using T = VOXEL_GET_TYPE(*GetData(DeclVal<ArrayType>()));
		checkStatic(TIsTriviallyDestructible<T>::Value);

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
				Count * sizeof(T));
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
					Count * sizeof(T));
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

	VOXELCORE_API bool AllEqual(uint8 Value, TConstArrayView<uint8> Data);
	VOXELCORE_API bool AllEqual(uint16 Value, TConstArrayView<uint16> Data);
	VOXELCORE_API bool AllEqual(uint32 Value, TConstArrayView<uint32> Data);
	VOXELCORE_API bool AllEqual(uint64 Value, TConstArrayView<uint64> Data);

	VOXELCORE_API float GetMin(TConstArrayView<float> Data);
	VOXELCORE_API float GetMax(TConstArrayView<float> Data);

	// Will return { MAX_int32, -MAX_int32 } if no values are valid
	VOXELCORE_API FInt32Interval GetMinMax(TConstArrayView<int32> Data);
	// Will return { 0, 0 } if no values are valid
	// Won't check for NaNs
	VOXELCORE_API FFloatInterval GetMinMax(TConstArrayView<float> Data);

	// Will return { MAX_flt, -MAX_flt } if no values are valid
	// Will skip NaNs and infinite values
	VOXELCORE_API FFloatInterval GetMinMaxSafe(TConstArrayView<float> Data);
	// Will return { MAX_dbl, -MAX_dbl } if no values are valid
	// Will skip NaNs and infinite values
	VOXELCORE_API FDoubleInterval GetMinMaxSafe(TConstArrayView<double> Data);
}