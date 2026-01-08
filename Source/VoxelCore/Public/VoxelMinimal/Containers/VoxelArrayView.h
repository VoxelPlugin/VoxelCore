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
		IsCompatibleElementType_V<std::remove_pointer_t<decltype(::GetData(std::declval<OtherRangeType>()))>>
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

	FORCEINLINE TVoxelArrayView GetRow3D(
		const FIntVector& Size,
		const int32 Y,
		const int32 Z) const
	{
		checkVoxelSlow(0 <= Y && Y < Size.Y);
		checkVoxelSlow(0 <= Z && Z < Size.Z);
		checkVoxelSlow(this->Num() == Size.X * Size.Y * Size.Z);

		return this->Slice(Y * Size.X + Z * Size.X * Size.Y, Size.X);
	}
	FORCEINLINE TVoxelArrayView GetRow3D(
		const int32 Size,
		const int32 Y,
		const int32 Z) const
	{
		return this->GetRow3D(FIntVector(Size), Y, Z);
	}

	FORCEINLINE void CopyTo(const TVoxelArrayView<std::remove_const_t<ElementType>> Other) const
	{
		checkVoxelSlow(this->Num() == Other.Num());
		checkStatic(std::is_trivially_destructible_v<ElementType>);

		FMemory::Memcpy(
			Other.GetData(),
			GetData(),
			Num() * sizeof(ElementType));
	}
	template<int32 Size>
	FORCEINLINE void CopyTo(const TVoxelArrayView<std::remove_const_t<ElementType>> Other) const
	{
		checkVoxelSlow(this->Num() == Size);
		checkVoxelSlow(this->Num() == Other.Num());
		checkStatic(std::is_trivially_destructible_v<ElementType>);

		FMemory::Memcpy(
			Other.GetData(),
			GetData(),
			Size * sizeof(ElementType));
	}

	void CopyTo3D(
		const FIntVector& Size,
		const TVoxelArrayView<std::remove_const_t<ElementType>> Other,
		const FIntVector& OffsetInOther,
		const FIntVector& OtherSize) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);
		checkVoxelSlow(Num() == Size.X * Size.Y * Size.Z);
		checkVoxelSlow(Other.Num() == OtherSize.X * OtherSize.Y * OtherSize.Z);

		checkVoxelSlow(0 <= OffsetInOther.X && OffsetInOther.X + Size.X <= OtherSize.X);
		checkVoxelSlow(0 <= OffsetInOther.Y && OffsetInOther.Y + Size.Y <= OtherSize.Y);
		checkVoxelSlow(0 <= OffsetInOther.Z && OffsetInOther.Z + Size.Z <= OtherSize.Z);

		for (int32 IndexZ = 0; IndexZ < Size.Z; IndexZ++)
		{
			for (int32 IndexY = 0; IndexY < Size.Y; IndexY++)
			{
				GetRow3D(Size, IndexY, IndexZ).CopyTo(Other
					.GetRow3D(
						OtherSize,
						OffsetInOther.Y + IndexY,
						OffsetInOther.Z + IndexZ)
					.Slice(OffsetInOther.X, Size.X));
			}
		}
	}
	template<int32 Size>
	void CopyTo3D(
		const TVoxelArrayView<std::remove_const_t<ElementType>> Other,
		const FIntVector& OffsetInOther,
		const FIntVector& OtherSize) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Size * Size * Size, 1024);
		checkVoxelSlow(Num() == Size * Size * Size);
		checkVoxelSlow(Other.Num() == OtherSize.X * OtherSize.Y * OtherSize.Z);

		checkVoxelSlow(0 <= OffsetInOther.X && OffsetInOther.X + Size <= OtherSize.X);
		checkVoxelSlow(0 <= OffsetInOther.Y && OffsetInOther.Y + Size <= OtherSize.Y);
		checkVoxelSlow(0 <= OffsetInOther.Z && OffsetInOther.Z + Size <= OtherSize.Z);

		for (int32 IndexZ = 0; IndexZ < Size; IndexZ++)
		{
			for (int32 IndexY = 0; IndexY < Size; IndexY++)
			{
				GetRow3D(Size, IndexY, IndexZ).template CopyTo<Size>(Other
					.GetRow3D(
						OtherSize,
						OffsetInOther.Y + IndexY,
						OffsetInOther.Z + IndexZ)
					.Slice(OffsetInOther.X, Size));
			}
		}
	}

	void CopyFrom3D(
		const FIntVector& Size,
		const TVoxelArrayView<const ElementType> Other,
		const FIntVector& OffsetInOther,
		const FIntVector& OtherSize) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Num(), 1024);
		checkVoxelSlow(Num() == Size.X * Size.Y * Size.Z);
		checkVoxelSlow(Other.Num() == OtherSize.X * OtherSize.Y * OtherSize.Z);

		checkVoxelSlow(0 <= OffsetInOther.X && OffsetInOther.X + Size.X <= OtherSize.X);
		checkVoxelSlow(0 <= OffsetInOther.Y && OffsetInOther.Y + Size.Y <= OtherSize.Y);
		checkVoxelSlow(0 <= OffsetInOther.Z && OffsetInOther.Z + Size.Z <= OtherSize.Z);

		for (int32 IndexZ = 0; IndexZ < Size.Z; IndexZ++)
		{
			for (int32 IndexY = 0; IndexY < Size.Y; IndexY++)
			{
				Other
				.GetRow3D(
					OtherSize,
					OffsetInOther.Y + IndexY,
					OffsetInOther.Z + IndexZ)
				.Slice(OffsetInOther.X, Size.X)
				.CopyTo(GetRow3D(Size, IndexY, IndexZ));
			}
		}
	}
	template<int32 Size>
	void CopyFrom3D(
		const TVoxelArrayView<const ElementType> Other,
		const FIntVector& OffsetInOther,
		const FIntVector& OtherSize) const
	{
		VOXEL_FUNCTION_COUNTER_NUM(Size * Size * Size, 1024);
		checkVoxelSlow(Num() == Size * Size * Size);
		checkVoxelSlow(Other.Num() == OtherSize.X * OtherSize.Y * OtherSize.Z);

		checkVoxelSlow(0 <= OffsetInOther.X && OffsetInOther.X + Size <= OtherSize.X);
		checkVoxelSlow(0 <= OffsetInOther.Y && OffsetInOther.Y + Size <= OtherSize.Y);
		checkVoxelSlow(0 <= OffsetInOther.Z && OffsetInOther.Z + Size <= OtherSize.Z);

		for (int32 IndexZ = 0; IndexZ < Size; IndexZ++)
		{
			for (int32 IndexY = 0; IndexY < Size; IndexY++)
			{
				Other
				.GetRow3D(
					OtherSize,
					OffsetInOther.Y + IndexY,
					OffsetInOther.Z + IndexZ)
				.Slice(OffsetInOther.X, Size)
				.template CopyTo<Size>(GetRow3D(Size, IndexY, IndexZ));
			}
		}
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
	template<typename T, typename ToType = std::conditional_t<std::is_const_v<ElementType>, const T, T>>
	FORCEINLINE ToType& ReinterpretAsSingle() const
	{
		const int64 NumBytes = Num() * sizeof(ElementType);
		checkVoxelSlow(NumBytes == sizeof(ToType));
		return *reinterpret_cast<ToType*>(GetData());
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

template <typename InElementType, typename InSizeType> constexpr bool TIsTArrayView_V<               TVoxelArrayView<InElementType, InSizeType>> = true;
template <typename InElementType, typename InSizeType> constexpr bool TIsTArrayView_V<      volatile TVoxelArrayView<InElementType, InSizeType>> = true;
template <typename InElementType, typename InSizeType> constexpr bool TIsTArrayView_V<const          TVoxelArrayView<InElementType, InSizeType>> = true;
template <typename InElementType, typename InSizeType> constexpr bool TIsTArrayView_V<const volatile TVoxelArrayView<InElementType, InSizeType>> = true;

template<typename T>
FORCEINLINE auto MakeVoxelArrayView(T&& Other)
{
	if constexpr (TIsArray<std::remove_reference_t<T>>::Value)
	{
		checkStatic(sizeof(Other) < sizeof(T) * MAX_int32);

		return TVoxelArrayView<
			std::remove_reference_t<decltype(*GetData(std::declval<T>()))>,
			int32>(Other);
	}
	else if constexpr (TIsContiguousContainer<T>::Value)
	{
		return TVoxelArrayView<
			std::remove_reference_t<decltype(*GetData(std::declval<T>()))>,
			decltype(GetNum(Other))>(Other);
	}
	else
	{
		return TVoxelArrayView<std::remove_reference_t<T>>(&Other, 1);
	}
}

template<typename ElementType, typename SizeType>
requires
(
	std::is_same_v<SizeType, int32> ||
	std::is_same_v<SizeType, int64>
)
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
struct TVoxelConstCast<TVoxelArrayView<const T, SizeType>>
{
	FORCEINLINE static TVoxelArrayView<T, SizeType>& ConstCast(TVoxelArrayView<const T, SizeType>& Value)
	{
		return ReinterpretCastRef<TVoxelArrayView<T, SizeType>>(Value);
	}
	FORCEINLINE static const TVoxelArrayView<T, SizeType>& ConstCast(const TVoxelArrayView<const T, SizeType>& Value)
	{
		return ReinterpretCastRef<TVoxelArrayView<T, SizeType>>(Value);
	}
};