// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArray.h"

template<typename IteratorType>
struct TVoxelDereferencingIterator
{
	using Type = std::remove_pointer_t<std::remove_reference_t<decltype(*std::declval<IteratorType&>())>>;

	mutable IteratorType Iterator;

	FORCEINLINE explicit TVoxelDereferencingIterator(IteratorType&& Iterator)
		: Iterator(Iterator)
	{
	}

	FORCEINLINE TVoxelDereferencingIterator& operator++()
	{
		++Iterator;
		return *this;
	}
	FORCEINLINE explicit operator bool() const
	{
		return bool(Iterator);
	}
	FORCEINLINE Type& operator*() const
	{
		Type* Value = *Iterator;
		checkVoxelSlow(Value);
		return *Value;
	}
	FORCEINLINE bool operator!=(const TVoxelDereferencingIterator& Other) const
	{
		return Iterator != Other.Iterator;
	}
};

template<typename IteratorType>
TVoxelDereferencingIterator<IteratorType> MakeDereferencingIterator(IteratorType&& Iterator)
{
	return TVoxelDereferencingIterator<IteratorType>(MoveTemp(Iterator));
}

template<typename T, typename = decltype(begin(std::declval<T&>()))>
FORCEINLINE auto VoxelDereferencingRange_CallBegin(T& Value)
{
	return begin(Value);
}
template<typename T, typename = decltype(end(std::declval<T&>()))>
FORCEINLINE auto VoxelDereferencingRange_CallEnd(T& Value)
{
	return end(Value);
}

template<typename T, typename = decltype(std::declval<T&>().begin()), typename = void>
FORCEINLINE auto VoxelDereferencingRange_CallBegin(T& Value)
{
	return Value.begin();
}
template<typename T, typename = decltype(std::declval<T&>().end()), typename = void>
FORCEINLINE auto VoxelDereferencingRange_CallEnd(T& Value)
{
	return Value.end();
}

template<typename RangeType>
class TVoxelDereferencingRange
{
public:
	using Type = typename decltype(MakeDereferencingIterator(VoxelDereferencingRange_CallBegin(std::declval<RangeType&>())))::Type;

	RangeType Range;

	FORCEINLINE TVoxelDereferencingRange(RangeType&& Range)
		: Range(MoveTemp(Range))
	{
	}

	TVoxelArray<Type*> Array() const
	{
		TVoxelArray<Type*> Result;
		for (Type& Value : *this)
		{
			Result.Add(&Value);
		}
		return Result;
	}
	int32 Num() const
	{
		int32 Result = 0;
		for (Type& Value : *this)
		{
			Result++;
		}
		return Result;
	}

	FORCEINLINE auto begin() const
	{
		return MakeDereferencingIterator(VoxelDereferencingRange_CallBegin(Range));
	}
	FORCEINLINE auto end() const
	{
		return MakeDereferencingIterator(VoxelDereferencingRange_CallEnd(Range));
	}
};

template<typename RangeType>
class TVoxelDereferencingRangeRef
{
public:
	RangeType& Range;

	FORCEINLINE TVoxelDereferencingRangeRef(RangeType& Range)
		: Range(Range)
	{
	}

	FORCEINLINE int32 Num() const
	{
		return Range.Num();
	}

	FORCEINLINE auto begin() const
	{
		return MakeDereferencingIterator(VoxelDereferencingRange_CallBegin(Range));
	}
	FORCEINLINE auto end() const
	{
		return MakeDereferencingIterator(VoxelDereferencingRange_CallEnd(Range));
	}
};