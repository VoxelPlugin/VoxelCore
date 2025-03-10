// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename ElementType, typename SizeType = int32>
class TVoxelArrayView;

template<typename InElementType, typename InAllocator = FDefaultAllocator>
class TVoxelArray;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename FromType, typename ToType>
struct TVoxelArrayView_IsCompatible : std::false_type
{
};

template<typename FromType, typename ToType>
requires
(
	!std::is_const_v<FromType>
)
struct TVoxelArrayView_IsCompatible<FromType, const ToType> : TVoxelArrayView_IsCompatible<FromType, ToType>
{
};

template<typename T>
struct TVoxelArrayView_IsCompatible<T, T> : std::true_type
{
};

template<typename FromType, typename ToType>
requires
(
	!std::is_same_v<FromType, std::remove_const_t<ToType>> &&
	sizeof(FromType) == sizeof(ToType) &&
	bool(TPointerIsConvertibleFromTo<FromType, ToType>::Value)
)
struct TVoxelArrayView_IsCompatible<FromType, ToType> : std::true_type
{
};

template<typename FromType, typename ToType>
requires
(
	!std::is_same_v<FromType, ToType> &&
	!std::is_same_v<FromType, std::remove_const_t<ToType>> &&
	bool(TPointerIsConvertibleFromTo<FromType, ToType>::Value)
)
struct TVoxelArrayView_IsCompatible<FromType*, ToType*> : std::true_type
{
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Switched range check to use checkVoxel
template<typename InElementType, typename InSizeType>
class TVoxelArrayView : public TArrayView<InElementType, InSizeType>
{
public:
	using Super = TArrayView<InElementType, InSizeType>;
	using typename Super::ElementType;
	using typename Super::SizeType;
	using Super::IsValidIndex;
	using Super::CheckInvariants;
	using Super::GetData;
	using Super::Num;

	template<typename T>
	static constexpr bool IsCompatibleElementType_V = TVoxelArrayView_IsCompatible<T, ElementType>::value;

	TVoxelArrayView() = default;

	template<typename OtherRangeType>
	requires
	(
		!std::is_same_v<OtherRangeType, TVoxelArrayView> &&
		bool(TIsContiguousContainer<std::remove_cv_t<std::remove_reference_t<OtherRangeType>>>::Value) &&
		IsCompatibleElementType_V<std::remove_pointer_t<decltype(::GetData(DeclVal<OtherRangeType>()))>>
	)
	FORCEINLINE TVoxelArrayView(OtherRangeType&& Other)
	{
		struct FDummy
		{
			ElementType* DataPtr;
			SizeType ArrayNum;
		};

		const auto OtherNum = GetNum(Other);
		checkVoxelSlow(0 <= int64(OtherNum) && int64(OtherNum) <= int64(TNumericLimits<SizeType>::Max()));

		reinterpret_cast<FDummy&>(*this).DataPtr = ReinterpretCastPtr<ElementType>(::GetData(Other));
		reinterpret_cast<FDummy&>(*this).ArrayNum = SizeType(OtherNum);
	}

	template<typename OtherElementType, typename OtherSizeType>
	requires IsCompatibleElementType_V<OtherElementType>
	FORCEINLINE TVoxelArrayView(OtherElementType* InData, const OtherSizeType InCount)
	{
		checkVoxelSlow(0 <= int64(InCount) && int64(InCount) <= int64(TNumericLimits<SizeType>::Max()));
		checkVoxelSlow(InCount >= 0);

		struct FDummy
		{
			ElementType* DataPtr;
			SizeType ArrayNum;
		};
		reinterpret_cast<FDummy&>(*this).DataPtr = InData;
		reinterpret_cast<FDummy&>(*this).ArrayNum = InCount;
	}

	template<typename OtherElementType>
	requires
	(
		std::is_const_v<ElementType> &&
		IsCompatibleElementType_V<OtherElementType>
	)
	FORCEINLINE TVoxelArrayView(const std::initializer_list<OtherElementType> Elements)
		: TVoxelArrayView(Elements.begin(), Elements.size())
	{
	}

	FORCEINLINE void RangeCheck(InSizeType Index) const
	{
		CheckInvariants();

		checkfVoxelSlow((Index >= 0) & (Index < Num()),TEXT("Array index out of bounds: %i from an array of size %i"),Index,Num()); // & for one branch
	}

	FORCEINLINE bool IsValidSlice(InSizeType Index, InSizeType InNum) const
	{
		if (InNum == 0)
		{
			// Allow zero-sized slices at Num()
			return 0 <= Index && Index <= Num();
		}

		return
			IsValidIndex(Index) &&
			IsValidIndex(Index + InNum - 1);
	}
	FORCEINLINE TVoxelArrayView Slice(InSizeType Index, InSizeType InNum) const
	{
		checkVoxelSlow(IsValidSlice(Index, InNum));
		return TVoxelArrayView(GetData() + Index, InNum);
	}

	// Index is excluded
	FORCEINLINE TVoxelArrayView LeftOf(const SizeType Index) const
	{
		return Slice(0, Index);
	}
	// Index is included
	FORCEINLINE TVoxelArrayView RightOf(const SizeType Index) const
	{
		return Slice(Index, Num() - Index);
	}

	FORCEINLINE ElementType& operator[](InSizeType Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	FORCEINLINE ElementType& Last(InSizeType IndexFromTheEnd = 0) const
	{
		RangeCheck(Num() - IndexFromTheEnd - 1);
		return GetData()[Num() - IndexFromTheEnd - 1];
	}

	template<typename T, typename ReturnSizeType = SizeType, typename ToType = std::conditional_t<std::is_const_v<ElementType>, const T, T>>
	FORCEINLINE TVoxelArrayView<ToType, ReturnSizeType> ReinterpretAs() const
	{
		const int64 NumBytes = Num() * sizeof(ElementType);
		checkVoxelSlow(NumBytes % sizeof(ToType) == 0);
		return TVoxelArrayView<ToType, ReturnSizeType>(reinterpret_cast<ToType*>(GetData()), NumBytes / sizeof(T));
	}

	TVoxelArray<std::remove_const_t<InElementType>> Array() const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 128);
		return TVoxelArray<std::remove_const_t<InElementType>>(GetData(), Num());
	}

	void Serialize(FArchive& Ar)
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 128);
		checkStatic(!std::is_const_v<ElementType>);
		checkStatic(std::is_trivially_destructible_v<ElementType>);

		Ar.Serialize(GetData(), Num() * sizeof(ElementType));
	}

private:
	using Super::Left;
	using Super::LeftChop;
	using Super::Right;
	using Super::RightChop;
};

template<typename T>
using TVoxelArrayView64 = TVoxelArrayView<T, int64>;

template<typename T, typename SizeType = int32>
using TConstVoxelArrayView = TVoxelArrayView<const T, SizeType>;

template<typename T>
using TConstVoxelArrayView64 = TConstVoxelArrayView<T, int64>;

template<typename InElementType>
struct TIsZeroConstructType<TVoxelArrayView<InElementType>> : TIsZeroConstructType<TArrayView<InElementType>>
{
};
template<typename T, typename SizeType>
struct TIsContiguousContainer<TVoxelArrayView<T, SizeType>> : TIsContiguousContainer<TArrayView<T, SizeType>>
{
};

template<typename InElementType, typename InSizeType> struct TIsTArrayView<               TVoxelArrayView<InElementType, InSizeType>> { static constexpr bool Value = true; };
template<typename InElementType, typename InSizeType> struct TIsTArrayView<      volatile TVoxelArrayView<InElementType, InSizeType>> { static constexpr bool Value = true; };
template<typename InElementType, typename InSizeType> struct TIsTArrayView<const          TVoxelArrayView<InElementType, InSizeType>> { static constexpr bool Value = true; };
template<typename InElementType, typename InSizeType> struct TIsTArrayView<const volatile TVoxelArrayView<InElementType, InSizeType>> { static constexpr bool Value = true; };

template<typename T>
FORCEINLINE auto MakeVoxelArrayView(T&& Other)
{
	if constexpr (TIsArray<std::remove_reference_t<T>>::Value)
	{
		checkStatic(sizeof(Other) < sizeof(T) * MAX_int32);

		return TVoxelArrayView<
			std::remove_reference_t<decltype(*GetData(DeclVal<T>()))>,
			int32>(Other);
	}
	else if constexpr (TIsContiguousContainer<T>::Value)
	{
		return TVoxelArrayView<
			std::remove_reference_t<decltype(*GetData(DeclVal<T>()))>,
			decltype(GetNum(Other))>(Other);
	}
	else
	{
		return TVoxelArrayView<std::remove_reference_t<T>>(&Other, 1);
	}
}

template<typename ElementType, typename SizeType, typename = std::enable_if_t<std::is_same_v<SizeType, int32> || std::is_same_v<SizeType, int64>>>
FORCEINLINE auto MakeVoxelArrayView(ElementType* Pointer, const SizeType Size)
{
	return TVoxelArrayView<ElementType, SizeType>(Pointer, Size);
}

template<typename T>
FORCEINLINE auto MakeByteVoxelArrayView(T&& Value)
{
	return MakeVoxelArrayView(Value).template ReinterpretAs<uint8, int64>();
}

template<typename T, typename SizeType>
FORCEINLINE T& FromByteVoxelArrayView(const TVoxelArrayView<uint8, SizeType> Array)
{
	checkVoxelSlow(Array.Num() == sizeof(T));
	return *reinterpret_cast<T*>(Array.GetData());
}
template<typename T, typename SizeType>
FORCEINLINE const T& FromByteVoxelArrayView(const TConstVoxelArrayView<uint8, SizeType> Array)
{
	checkVoxelSlow(Array.Num() == sizeof(T));
	return *reinterpret_cast<const T*>(Array.GetData());
}

template<typename T, typename SizeType>
struct TVoxelConstCast<TVoxelArrayView<const T, SizeType>>
{
	FORCEINLINE static const TVoxelArrayView<T, SizeType>& ConstCast(const TVoxelArrayView<const T, SizeType>& Value)
	{
		return ReinterpretCastRef<TVoxelArrayView<T, SizeType>>(Value);
	}
};