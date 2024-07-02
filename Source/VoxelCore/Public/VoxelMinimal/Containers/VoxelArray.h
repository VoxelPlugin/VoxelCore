// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Templates/MakeUnsigned.h"
#include "VoxelMinimal/VoxelMemory.h"

template<typename InElementType, typename InAllocator = FVoxelAllocator>
class TVoxelArray : public TArray<InElementType, InAllocator>
{
public:
	using Super = TArray<InElementType, InAllocator>;
	using typename Super::SizeType;
	using typename Super::ElementType;
	using typename Super::AllocatorType;
	using Super::GetData;
	using Super::IsValidIndex;
	using Super::Shrink;
	using Super::ArrayNum;
	using Super::ArrayMax;
	using Super::SwapMemory;
	using Super::AllocatorInstance;
	using Super::Num;
	using Super::Super;

	using USizeType = typename TMakeUnsigned<SizeType>::Type;

#if !INTELLISENSE_PARSER
	// Copy constructors are inherited on MSVC, but implementing them manually deletes all the other inherited constructors
#if !defined(_MSC_VER) || _MSC_VER > 1929 || PLATFORM_COMPILER_CLANG
	template<typename OtherElementType, typename OtherAllocator>
	FORCEINLINE explicit TVoxelArray(const TArray<OtherElementType, OtherAllocator>& Other)
		: Super(Other)
	{
	}
#endif
#endif

	template<typename OtherAllocator>
	FORCEINLINE TVoxelArray& operator=(const TArray<ElementType, OtherAllocator>& Other)
	{
		Super::operator=(Other);
		return *this;
	}

public:
	FORCEINLINE void CheckInvariants() const
	{
		checkVoxelSlow((ArrayNum >= 0) && (ArrayMax >= ArrayNum));
	}
	FORCEINLINE void RangeCheck(SizeType Index) const
	{
		CheckInvariants();
		checkVoxelSlow((Index >= 0) && (Index < ArrayNum));
	}
	FORCEINLINE void CheckAddress(const ElementType* Address) const
	{
		checkVoxelSlow(Address < GetData() || Address >= (GetData() + ArrayMax));
	}

public:
	// Cannot be used to change the allocation!
	FORCEINLINE const TArray<InElementType>& ToConstArray() const
	{
		return ReinterpretCastRef<TArray<InElementType>>(*this);
	}

	FORCEINLINE ElementType Pop()
	{
		RangeCheck(0);
		ElementType Result = MoveTemp(GetData()[ArrayNum - 1]);

		DestructItem(GetData() + ArrayNum - 1);
		ArrayNum--;

		return Result;
	}

	FORCEINLINE SizeType Add_NoGrow(const ElementType& Item)
	{
		CheckAddress(&Item);
		checkVoxelSlow(ArrayNum < ArrayMax);

		const SizeType Index = ArrayNum++;

		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Item);
		return Index;
	}
	FORCEINLINE SizeType Add_EnsureNoGrow(const ElementType& Item)
	{
		ensureVoxelSlow(ArrayNum < ArrayMax);
		return this->Add(Item);
	}

	FORCEINLINE SizeType Add(ElementType&& Item)
	{
		CheckAddress(&Item);
		return this->Emplace(MoveTempIfPossible(Item));
	}
	FORCEINLINE SizeType Add(const ElementType& Item)
	{
		CheckAddress(&Item);
		return this->Emplace(Item);
	}
	FORCEINLINE ElementType& Add_GetRef(ElementType&& Item)
	{
		CheckAddress(&Item);
		return this->Emplace_GetRef(MoveTempIfPossible(Item));
	}
	FORCEINLINE ElementType& Add_GetRef(const ElementType& Item)
	{
		CheckAddress(&Item);
		return this->Emplace_GetRef(Item);
	}

	template<typename... ArgsType>
	FORCEINLINE SizeType Emplace_NoGrow(ArgsType&&... Args)
	{
		checkVoxelSlow(ArrayNum < ArrayMax);

		const SizeType Index = ArrayNum++;

		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Forward<ArgsType>(Args)...);
		return Index;
	}

	template<typename... ArgsType>
	FORCEINLINE SizeType Emplace(ArgsType&&... Args)
	{
		const SizeType Index = AddUninitialized();
		new (GetData() + Index) ElementType(Forward<ArgsType>(Args)...);
		return Index;
	}
	template<typename... ArgsType>
	FORCEINLINE ElementType& Emplace_GetRef(ArgsType&&... Args)
	{
		const SizeType Index = AddUninitialized();
		ElementType* Ptr = GetData() + Index;
		new (Ptr) ElementType(Forward<ArgsType>(Args)...);
		return *Ptr;
	}
	template<typename... ArgsType>
	FORCEINLINE ElementType& Emplace_GetRef_EnsureNoGrow(ArgsType&&... Args)
	{
		ensureVoxelSlow(ArrayNum < ArrayMax);
		return this->Emplace_GetRef(Forward<ArgsType>(Args)...);
	}

	FORCEINLINE ElementType& Last(SizeType IndexFromTheEnd = 0)
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}
	FORCEINLINE const ElementType& Last(SizeType IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	FORCEINLINE SizeType Find(const ElementType& Item) const
	{
		const ElementType* RESTRICT Start = GetData();
		const ElementType* RESTRICT End = Start + ArrayNum;
		for (const ElementType* RESTRICT Data = Start; Data != End; ++Data)
		{
			if (*Data == Item)
			{
				return static_cast<SizeType>(Data - Start);
			}
		}
		return INDEX_NONE;
	}
	template<typename ComparisonType>
	FORCEINLINE bool Contains(const ComparisonType& Item) const
	{
		const ElementType* RESTRICT Start = GetData();
		const ElementType* RESTRICT End = Start + ArrayNum;
		for (const ElementType* RESTRICT Data = Start; Data != End; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE SizeType AddUnique(const ElementType& Item)
	{
		const SizeType Index = this->Find(Item);
		if (Index != -1)
		{
			return Index;
		}

		return this->Add(Item);
	}
	FORCEINLINE SizeType AddUninitialized(SizeType Count = 1)
	{
		CheckInvariants();
		checkVoxelSlow(Count >= 0);

		const USizeType OldNum = USizeType(ArrayNum);
		const USizeType NewNum = OldNum + USizeType(Count);
		ArrayNum = SizeType(NewNum);

		if (NewNum > USizeType(ArrayMax))
		{
			this->ResizeGrow(SizeType(OldNum));
		}
		return OldNum;
	}
	FORCEINLINE void Reserve(const SizeType Number)
	{
		checkVoxelSlow(Number >= 0);

		if (Number > ArrayMax)
		{
			this->ResizeTo(Number);
		}
	}

public:
	FORCEINLINE void RemoveAt(SizeType Index)
	{
		RemoveAt(Index, 1);
	}
	template<typename CountType>
	FORCEINLINE void RemoveAt(SizeType Index, CountType Count)
	{
		checkStatic(!std::is_same_v<CountType, bool>);
		checkVoxelSlow(Count > 0);

		RangeCheck(Index);
		RangeCheck(Index + Count - 1);

		DestructItems(GetData() + Index, Count);

		const SizeType NumToMove = ArrayNum - (Index + Count);
		if (NumToMove)
		{
			FMemory::Memmove
			(
				GetData() + Index,
				GetData() + Index + Count,
				NumToMove * sizeof(ElementType)
			);
		}
		ArrayNum -= Count;
	}
	FORCEINLINE void RemoveAtSwap(SizeType Index)
	{
		RangeCheck(Index);

		DestructItem(GetData() + Index);

		if (Index != ArrayNum - 1)
		{
			FMemory::Memcpy
			(
				GetData() + Index,
				GetData() + ArrayNum - 1,
				sizeof(ElementType)
			);
		}

		ArrayNum--;
	}

private:
	FORCENOINLINE void ResizeGrow(const SizeType OldNum)
	{
		checkVoxelSlow(OldNum < ArrayNum);

		ArrayMax = this->AllocatorCalculateSlackGrow(ArrayNum, ArrayMax);
		this->AllocatorResizeAllocation(OldNum, ArrayMax);
	}
	FORCENOINLINE void ResizeTo(SizeType NewMax)
	{
		if (NewMax)
		{
			NewMax = this->AllocatorCalculateSlackReserve(NewMax);
		}
		if (NewMax != ArrayMax)
		{
			ArrayMax = NewMax;
			this->AllocatorResizeAllocation(ArrayNum, ArrayMax);
		}
	}
	FORCENOINLINE void AllocatorResizeAllocation(const SizeType CurrentArrayNum, const SizeType NewArrayMax)
	{
		VOXEL_FUNCTION_COUNTER_NUM(NewArrayMax, 1024);

		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			AllocatorInstance.ResizeAllocation(CurrentArrayNum, NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			AllocatorInstance.ResizeAllocation(CurrentArrayNum, NewArrayMax, sizeof(ElementType));
		}
	}
	FORCEINLINE SizeType AllocatorCalculateSlackGrow(const SizeType CurrentArrayNum, const SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			return AllocatorInstance.CalculateSlackGrow(CurrentArrayNum, NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return AllocatorInstance.CalculateSlackGrow(CurrentArrayNum, NewArrayMax, sizeof(ElementType));
		}
	}
	FORCEINLINE SizeType AllocatorCalculateSlackReserve(const SizeType NewArrayMax)
	{
		if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
		{
			return AllocatorInstance.CalculateSlackReserve(NewArrayMax, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			return AllocatorInstance.CalculateSlackReserve(NewArrayMax, sizeof(ElementType));
		}
	}

public:
	FORCEINLINE void Swap(const SizeType FirstIndexToSwap, const SizeType SecondIndexToSwap)
	{
		checkVoxelSlow(IsValidIndex(FirstIndexToSwap));
		checkVoxelSlow(IsValidIndex(SecondIndexToSwap));
		this->SwapMemory(FirstIndexToSwap, SecondIndexToSwap);
	}

public:
	FORCEINLINE ElementType& operator[](SizeType Index)
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	FORCEINLINE const ElementType& operator[](SizeType Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

public:
#if TARRAY_RANGED_FOR_CHECKS && VOXEL_DEBUG
	typedef TCheckedPointerIterator<      ElementType, SizeType> RangedForIteratorType;
	typedef TCheckedPointerIterator<const ElementType, SizeType> RangedForConstIteratorType;
#else
	typedef       ElementType* RangedForIteratorType;
	typedef const ElementType* RangedForConstIteratorType;
#endif

#if TARRAY_RANGED_FOR_CHECKS && VOXEL_DEBUG
	FORCEINLINE RangedForIteratorType      begin() { return RangedForIteratorType(ArrayNum, GetData()); }
	FORCEINLINE RangedForConstIteratorType begin() const { return RangedForConstIteratorType(ArrayNum, GetData()); }
	FORCEINLINE RangedForIteratorType      end() { return RangedForIteratorType(ArrayNum, GetData() + Num()); }
	FORCEINLINE RangedForConstIteratorType end() const { return RangedForConstIteratorType(ArrayNum, GetData() + Num()); }
#else
	FORCEINLINE RangedForIteratorType      begin() { return GetData(); }
	FORCEINLINE RangedForConstIteratorType begin() const { return GetData(); }
	FORCEINLINE RangedForIteratorType      end() { return GetData() + Num(); }
	FORCEINLINE RangedForConstIteratorType end() const { return GetData() + Num(); }
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename To, typename ElementType, typename Allocator, typename = std::enable_if_t<sizeof(To) == sizeof(ElementType)>>
FORCEINLINE TVoxelArray<To, Allocator>& ReinterpretCastVoxelArray(TVoxelArray<ElementType, Allocator>& Array)
{
	return reinterpret_cast<TVoxelArray<To, Allocator>&>(Array);
}
template<typename To, typename ElementType, typename Allocator, typename = std::enable_if_t<sizeof(To) == sizeof(ElementType)>>
FORCEINLINE TVoxelArray<To, Allocator>&& ReinterpretCastVoxelArray(TVoxelArray<ElementType, Allocator>&& Array)
{
	return reinterpret_cast<TVoxelArray<To, Allocator>&&>(Array);
}

template<typename To, typename ElementType, typename Allocator, typename = std::enable_if_t<sizeof(To) == sizeof(ElementType)>>
FORCEINLINE const TVoxelArray<To, Allocator>& ReinterpretCastVoxelArray(const TVoxelArray<ElementType, Allocator>& Array)
{
	return reinterpret_cast<const TVoxelArray<To, Allocator>&>(Array);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename ToType, typename ToAllocator, typename FromType, typename Allocator, typename = std::enable_if_t<sizeof(FromType) != sizeof(ToType)>>
TVoxelArray<ToType, ToAllocator> ReinterpretCastVoxelArray_Copy(const TVoxelArray<FromType, Allocator>& Array)
{
	const int64 NumBytes = Array.Num() * sizeof(FromType);
	check(NumBytes % sizeof(ToType) == 0);
	return TVoxelArray<ToType, Allocator>(reinterpret_cast<const ToType*>(Array.GetData()), NumBytes / sizeof(ToType));
}
template<typename ToType, typename FromType, typename Allocator, typename = std::enable_if_t<sizeof(FromType) != sizeof(ToType)>>
TVoxelArray<ToType, Allocator> ReinterpretCastVoxelArray_Copy(const TVoxelArray<FromType, Allocator>& Array)
{
	return ReinterpretCastVoxelArray_Copy<ToType, Allocator>(Array);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename InElementType, typename Allocator>
struct TIsZeroConstructType<TVoxelArray<InElementType, Allocator>> : TIsZeroConstructType<TArray<InElementType, Allocator>>
{
};

template<typename T, typename Allocator>
struct TIsContiguousContainer<TVoxelArray<T, Allocator>> : TIsContiguousContainer<TArray<T, Allocator>>
{
};

template<typename InElementType, typename Allocator> struct TIsTArray<               TVoxelArray<InElementType, Allocator>> { enum { Value = true }; };
template<typename InElementType, typename Allocator> struct TIsTArray<const          TVoxelArray<InElementType, Allocator>> { enum { Value = true }; };
template<typename InElementType, typename Allocator> struct TIsTArray<      volatile TVoxelArray<InElementType, Allocator>> { enum { Value = true }; };
template<typename InElementType, typename Allocator> struct TIsTArray<const volatile TVoxelArray<InElementType, Allocator>> { enum { Value = true }; };

template<typename T>
using TVoxelArray64 = TVoxelArray<T, FVoxelAllocator64>;

template<typename T, int32 NumInlineElements>
using TVoxelInlineArray = TVoxelArray<T, TVoxelInlineAllocator<NumInlineElements>>;

template<typename T, int32 NumInlineElements>
using TVoxelFixedArray = TVoxelArray<T, TFixedAllocator<NumInlineElements>>;