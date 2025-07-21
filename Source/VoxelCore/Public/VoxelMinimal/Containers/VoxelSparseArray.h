// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelTypeCompatibleBytes.h"
#include "VoxelMinimal/Containers/VoxelArray.h"
#include "VoxelMinimal/Containers/VoxelBitArray.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

template<typename Type>
class TVoxelSparseArray
{
private:
	union FValue
	{
		TVoxelTypeCompatibleBytes<Type> Value;
		int32 NextFreeIndex = -1;
	};

public:
	TVoxelSparseArray() = default;
	FORCEINLINE ~TVoxelSparseArray()
	{
		Reset();
	}

	TVoxelSparseArray(TVoxelSparseArray&& Other)
	{
		ArrayNum = Other.ArrayNum;
		FirstFreeIndex = Other.FirstFreeIndex;
		AllocationFlags = MoveTemp(AllocationFlags);
		Values = MoveTemp(Values);

		Other.ArrayNum = 0;
		Other.FirstFreeIndex = -1;
	}
	TVoxelSparseArray(const TVoxelSparseArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		ArrayNum = Other.Num();
		AllocationFlags.SetNum(Other.Num(), true);
		Values.Reserve(Other.Num());

		for (const Type& Value : Other)
		{
			new(Values.Emplace_GetRef_EnsureNoGrow().Value) Type(Value);
		}
	}

	TVoxelSparseArray& operator=(TVoxelSparseArray&& Other)
	{
		Empty();
		new(this) TVoxelSparseArray(MoveTemp(Other));
		return *this;
	}
	TVoxelSparseArray& operator=(const TVoxelSparseArray& Other)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Other.Num(), 1024);

		Reset();

		ArrayNum = Other.Num();
		AllocationFlags.SetNum(Other.Num(), true);
		Values.Reserve(Other.Num());

		for (const Type& Value : Other)
		{
			new(Values.Emplace_GetRef_EnsureNoGrow().Value) Type(Value);
		}

		return *this;
	}

public:
	void Reserve(const int32 Number)
	{
		AllocationFlags.Reserve(Number);
		Values.Reserve(Number);
	}
	void Reset()
	{
		if (!std::is_trivially_destructible_v<Type>)
		{
			VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);

			for (Type& Value : *this)
			{
				Value.~Type();
			}
		}

		ArrayNum = 0;
		FirstFreeIndex = -1;
		AllocationFlags.Reset();
		Values.Reset();
	}
	void Empty()
	{
		Reset();

		AllocationFlags.Empty();
		Values.Empty();
	}
	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE int32 Max_Unsafe() const
	{
		return AllocationFlags.Num();
	}
	FORCEINLINE int64 GetAllocatedSize() const
	{
		return
			AllocationFlags.GetAllocatedSize() +
			Values.GetAllocatedSize();
	}
	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		checkVoxelSlow(AllocationFlags.Num() == Values.Num());

		if (!Values.IsValidIndex(Index))
		{
			return false;
		}

		return AllocationFlags[Index];
	}
	FORCEINLINE bool IsAllocated(const int32 Index) const
	{
		checkVoxelSlow(AllocationFlags.Num() == Values.Num());
		return AllocationFlags[Index];
	}

public:
	FORCEINLINE int32 Add(const Type& Value)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(Value);
		return Index;
	}
	FORCEINLINE int32 Add(Type&& Value)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(MoveTemp(Value));
		return Index;
	}

	template<typename... ArgTypes>
	requires std::is_constructible_v<Type, ArgTypes...>
	FORCEINLINE int32 Emplace(ArgTypes&&... Args)
	{
		const int32 Index = this->AddUninitialized();
		new (&(*this)[Index]) Type(Forward<ArgTypes>(Args)...);
		return Index;
	}
	template<typename... ArgTypes>
	requires std::is_constructible_v<Type, ArgTypes...>
	FORCEINLINE Type& Emplace_GetRef(ArgTypes&&... Args)
	{
		const int32 Index = this->AddUninitialized();
		return *new (&(*this)[Index]) Type(Forward<ArgTypes>(Args)...);
	}

	FORCEINLINE void RemoveAt(const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		ArrayNum--;
		checkVoxelSlow(ArrayNum >= 0);

		checkVoxelSlow(AllocationFlags[Index]);
		AllocationFlags[Index] = false;

		FValue& Value = Values[Index];

		ReinterpretCastRef<Type>(Value.Value).~Type();

		Value.NextFreeIndex = FirstFreeIndex;
		FirstFreeIndex = Index;
	}
	FORCEINLINE Type RemoveAt_ReturnValue(const int32 Index)
	{
		Type Value = MoveTemp((*this)[Index]);
		RemoveAt(Index);
		return MoveTemp(Value);
	}

public:
	FORCEINLINE Type& operator[](const int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));

		return ReinterpretCastRef<Type>(Values[Index].Value);
	}
	FORCEINLINE const Type& operator[](const int32 Index) const
	{
		return ConstCast(this)->operator[](Index);
	}

public:
	template<typename InType>
	struct TIterator : TVoxelRangeIterator<TIterator<InType>>
	{
	public:
		TIterator() = default;
		FORCEINLINE explicit TIterator(std::conditional_t<std::is_const_v<InType>, const TVoxelSparseArray, TVoxelSparseArray>& Array)
		{
			if (Array.Num() == 0)
			{
				// No need to iterate the full array
				return;
			}

			Values = Array.Values;
			Iterator = FVoxelSetBitIterator(Array.AllocationFlags.View());
		}

		FORCEINLINE InType& operator*() const
		{
			return ReinterpretCastRef<InType>(Values[Iterator.GetIndex()]);
		}
		FORCEINLINE void operator++()
		{
			++Iterator;
		}
		FORCEINLINE operator bool() const
		{
			return bool(Iterator);
		}

	private:
		TVoxelArrayView<std::conditional_t<std::is_const_v<InType>, const FValue, FValue>> Values;
		FVoxelSetBitIterator Iterator;
	};
	using FIterator = TIterator<Type>;
	using FConstIterator = TIterator<const Type>;

	FORCEINLINE FIterator begin()
	{
		return FIterator(*this);
	}
	FORCEINLINE FConstIterator begin() const
	{
		return FConstIterator(*this);
	}
	FORCEINLINE FVoxelRangeIteratorEnd end() const
	{
		return FIterator::end();
	}

private:
	int32 ArrayNum = 0;
	int32 FirstFreeIndex = -1;

	FVoxelBitArray AllocationFlags;
	TVoxelArray<FValue> Values;

	FORCEINLINE int32 AddUninitialized()
	{
		ArrayNum++;

		if (FirstFreeIndex != -1)
		{
			const int32 Index = FirstFreeIndex;

			FValue& Value = Values[Index];

			FirstFreeIndex = Value.NextFreeIndex;

			checkVoxelSlow(!AllocationFlags[Index]);
			AllocationFlags[Index] = true;

			return Index;
		}
		else
		{
			const int32 Index0 = AllocationFlags.Add(true);
			const int32 Index1 = Values.AddUninitialized();

			checkVoxelSlow(Index0 == Index1);
			return Index0;
		}
	}
};