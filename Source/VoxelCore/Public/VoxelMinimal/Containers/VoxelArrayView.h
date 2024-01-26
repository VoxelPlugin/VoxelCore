// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

template<typename ElementType, typename SizeType = int32>
class TVoxelArrayView;

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
	struct TIsCompatibleElementType
	{
		/** NOTE:
		 * The stars in the TPointerIsConvertibleFromTo test are *IMPORTANT*
		 * They prevent TArrayView<Base>(TArray<Derived>&) from compiling!
		 */
		static constexpr bool Value =
			TPointerIsConvertibleFromTo<T*, ElementType* const>::Value ||
			(
				(!TIsConst<T>::Value || TIsConst<ElementType>::Value) &&
				TIsPointer<T>::Value &&
				TIsPointer<ElementType>::Value &&
				TPointerIsConvertibleFromTo<
					typename TRemovePointer<std::remove_cv_t<T>>::Type,
					typename TRemovePointer<std::remove_cv_t<ElementType>>::Type>::Value
			);
	};

	TVoxelArrayView() = default;

	template<
		typename OtherRangeType,
		typename = typename TEnableIf<
			!std::is_same_v<OtherRangeType, TVoxelArrayView> &&
			TIsContiguousContainer<std::remove_cv_t<typename TRemoveReference<OtherRangeType>::Type>>::Value &&
			TIsCompatibleElementType<typename TRemovePointer<decltype(::GetData(DeclVal<OtherRangeType>()))>::Type>::Value
		>::Type
	>
	FORCEINLINE TVoxelArrayView(OtherRangeType&& Other)
	{
		struct FDummy
		{
			ElementType* DataPtr;
			SizeType ArrayNum;
		};

		const auto OtherNum = GetNum(Other);
		checkVoxelSlow(0 <= OtherNum && OtherNum <= TNumericLimits<SizeType>::Max());

		reinterpret_cast<FDummy&>(*this).DataPtr = ReinterpretCastPtr<ElementType>(::GetData(Other));
		reinterpret_cast<FDummy&>(*this).ArrayNum = SizeType(OtherNum);
	}

	template<typename OtherElementType, typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TVoxelArrayView(OtherElementType* InData, SizeType InCount)
	{
		struct FDummy
		{
			ElementType* DataPtr;
			SizeType ArrayNum;
		};
		reinterpret_cast<FDummy&>(*this).DataPtr = InData;
		reinterpret_cast<FDummy&>(*this).ArrayNum = InCount;

		checkVoxelSlow(InCount >= 0);
	}

	template<typename OtherElementType, typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value && std::is_const_v<ElementType>>::Type>
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
		return
			InNum >= 0 &&
			IsValidIndex(Index) &&
			IsValidIndex(Index + InNum - 1);
	}
	FORCEINLINE TVoxelArrayView Slice(InSizeType Index, InSizeType InNum) const
	{
		checkVoxelSlow(IsValidSlice(Index, InNum));
		return TVoxelArrayView(GetData() + Index, InNum);
	}

	// Returns the left-most part of the view by taking the given number of elements from the left
	FORCEINLINE TVoxelArrayView Left(const SizeType Count) const
	{
		return Slice(0, Count);
	}
	// Returns the left-most part of the view by chopping the given number of elements from the right
	FORCEINLINE TVoxelArrayView LeftChop(const SizeType Count) const
	{
		return Slice(0, Num() - Count);
	}
	// Returns the right-most part of the view by taking the given number of elements from the right
	FORCEINLINE TVoxelArrayView Right(const SizeType Count) const
	{
		return Slice(Num() - Count, Count);
	}
	// Returns the right-most part of the view by chopping the given number of elements from the left
	FORCEINLINE TVoxelArrayView RightChop(const SizeType Count) const
	{
		return Slice(Count, Num() - Count);
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
	if constexpr (TIsContiguousContainer<T>::Value)
	{
		return TVoxelArrayView<
			typename TRemoveReference<decltype(*GetData(DeclVal<T>()))>::Type,
			decltype(GetNum(Other))>(Other);
	}
	else
	{
		return TVoxelArrayView<typename TRemoveReference<T>::Type>(&Other, 1);
	}
}

template<typename ElementType, typename SizeType, typename = typename TEnableIf<std::is_same_v<SizeType, int32> || std::is_same_v<SizeType, int64>>::Type>
FORCEINLINE auto MakeVoxelArrayView(ElementType* Pointer, const SizeType Size)
{
	return TVoxelArrayView<ElementType, SizeType>(Pointer, Size);
}

template<typename ToType, typename FromType, typename SizeType>
FORCEINLINE TVoxelArrayView<ToType, SizeType> ReinterpretCastVoxelArrayView(TVoxelArrayView<FromType, SizeType> ArrayView)
{
	const int64 NumBytes = ArrayView.Num() * sizeof(FromType);
	checkVoxelSlow(NumBytes % sizeof(ToType) == 0);
	return TVoxelArrayView<ToType, SizeType>(reinterpret_cast<ToType*>(ArrayView.GetData()), NumBytes / sizeof(ToType));
}
template<typename ToType, typename FromType, typename SizeType, typename = typename TEnableIf<!TIsConst<ToType>::Value>::Type>
FORCEINLINE TVoxelArrayView<const ToType, SizeType> ReinterpretCastVoxelArrayView(TVoxelArrayView<const FromType, SizeType> ArrayView)
{
	return ReinterpretCastVoxelArrayView<const ToType>(ArrayView);
}

template<typename ToType, typename FromType, typename AllocatorType>
FORCEINLINE TVoxelArrayView<ToType, typename AllocatorType::SizeType> ReinterpretCastVoxelArrayView(TArray<FromType, AllocatorType>& Array)
{
	return ReinterpretCastVoxelArrayView<ToType>(MakeVoxelArrayView(Array));
}
template<typename ToType, typename FromType, typename AllocatorType>
FORCEINLINE TVoxelArrayView<const ToType, typename AllocatorType::SizeType> ReinterpretCastVoxelArrayView(const TArray<FromType, AllocatorType>& Array)
{
	return ReinterpretCastVoxelArrayView<const ToType>(MakeVoxelArrayView(Array));
}

template<typename T>
FORCEINLINE auto MakeByteVoxelArrayView(T&& Value)
{
	return ReinterpretCastVoxelArrayView<uint8>(MakeVoxelArrayView(Value));
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