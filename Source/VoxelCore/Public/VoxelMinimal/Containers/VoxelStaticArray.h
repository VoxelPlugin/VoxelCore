// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelTypeUtilities.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

template<typename T, int32 Size, int32 Alignment>
class alignas(Alignment) TVoxelStaticArrayBase
{
public:
	TVoxelStaticArrayBase() = default;

	FORCEINLINE static constexpr int32 Num()
	{
		return Size;
	}
	FORCEINLINE static constexpr int32 GetTypeSize()
	{
		return sizeof(T);
	}
	FORCEINLINE static constexpr int64 GetAllocatedSize()
	{
		return sizeof(TVoxelStaticArrayBase);
	}

	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < Num();
	}

	FORCEINLINE T* GetData()
	{
		return reinterpret_cast<T*>(Data);
	}
	FORCEINLINE const T* GetData() const
	{
		return reinterpret_cast<const T*>(Data);
	}

	FORCEINLINE T& operator[](int32 Index)
	{
		checkVoxelSlow(IsValidIndex(Index));
		return GetData()[Index];
	}
	FORCEINLINE const T& operator[](int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return GetData()[Index];
	}

	FORCEINLINE TVoxelArrayView<T> View()
	{
		return TVoxelArrayView<T>(GetData(), Num());
	}
	FORCEINLINE TConstVoxelArrayView<T> View() const
	{
		return TConstVoxelArrayView<T>(GetData(), Num());
	}

	FORCEINLINE operator TVoxelArrayView<T>()
	{
		return View();
	}
	FORCEINLINE operator TVoxelArrayView<const T>() const
	{
		return View();
	}

	FORCEINLINE T* begin()
	{
		return GetData();
	}
	FORCEINLINE T* end()
	{
		return GetData() + Size;
	}

	FORCEINLINE const T* begin() const
	{
		return GetData();
	}
	FORCEINLINE const T* end()   const
	{
		return GetData() + Size;
	}

	FORCEINLINE bool operator==(const TVoxelStaticArrayBase& Other) const
	{
		return CompareItems(GetData(), Other.GetData(), Num());
	}

private:
	uint8 Data[Size * sizeof(T)];
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T, int32 Size, int32 Alignment = alignof(T)>
class alignas(Alignment) TVoxelStaticArray;

template<typename T, int32 Size, int32 Alignment>
requires std::is_trivially_destructible_v<T>
class alignas(Alignment) TVoxelStaticArray<T, Size, Alignment> : public TVoxelStaticArrayBase<T, Size, Alignment>
{
public:
	using ElementType = T;

	FORCEINLINE explicit TVoxelStaticArray(EForceInit)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T{ FVoxelUtilities::MakeSafe<T>() };
		}
	}
	FORCEINLINE explicit TVoxelStaticArray(ENoInit)
	{
#if VOXEL_DEBUG
		FVoxelUtilities::Memset(*this, 0xDE);
#endif
	}

	FORCEINLINE explicit TVoxelStaticArray(T Value)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T(Value);
		}
	}
	template<typename... ArgTypes>
	requires
	(
		sizeof...(ArgTypes) == Size &&
		Size != 1 &&
		(std::is_constructible_v<T, ArgTypes&&> && ...)
	)
	FORCEINLINE TVoxelStaticArray(ArgTypes&&... Args)
	{
		int32 Index = 0;
		VOXEL_FOLD_EXPRESSION(new (this->GetData() + (Index++)) T(Forward<ArgTypes>(Args)));
		checkVoxelSlow(Index == Size);
	}

	FORCEINLINE TVoxelStaticArray(const TVoxelStaticArray& Other)
	{
		FMemory::Memcpy(this->GetData(), Other.GetData(), Size * sizeof(T));
	}

	FORCEINLINE TVoxelStaticArray& operator=(const TVoxelStaticArray& Other)
	{
		FMemory::Memcpy(this->GetData(), Other.GetData(), Size * sizeof(T));
		return *this;
	}

	FORCEINLINE void Memzero()
	{
		FMemory::Memzero(this->GetData(), Size * sizeof(T));
	}
	FORCEINLINE void Memset(const uint8 Value)
	{
		FMemory::Memset(this->GetData(), Value, Size * sizeof(T));
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TVoxelStaticArray& Array)
	{
		Ar.Serialize(Array.GetData(), Size * sizeof(T));
		return Ar;
	}
};

template<typename T, int32 Size, int32 Alignment>
requires
(
	!std::is_trivially_destructible_v<T>
)
class alignas(Alignment) TVoxelStaticArray<T, Size, Alignment> : public TVoxelStaticArrayBase<T, Size, Alignment>
{
public:
	using ElementType = T;

	FORCEINLINE TVoxelStaticArray()
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T{};
		}
	}
	FORCEINLINE explicit TVoxelStaticArray(T Value)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T(Value);
		}
	}
	template<typename... ArgTypes>
	requires
	(
		sizeof...(ArgTypes) == Size &&
		Size != 1 &&
		(std::is_constructible_v<T, ArgTypes&&> && ...)
	)
	FORCEINLINE TVoxelStaticArray(ArgTypes&&... Args)
	{
		int32 Index = 0;
		VOXEL_FOLD_EXPRESSION(new (this->GetData() + (Index++)) T(Forward<ArgTypes>(Args)));
		checkVoxelSlow(Index == Size);
	}
	FORCEINLINE TVoxelStaticArray(const TVoxelStaticArray& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T(Other[Index]);
		}
	}
	FORCEINLINE TVoxelStaticArray(TVoxelStaticArray&& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (this->GetData() + Index) T(MoveTemp(Other[Index]));
		}
	}
	FORCEINLINE ~TVoxelStaticArray()
	{
		for (T& Element : *this)
		{
			Element.~T();
		}
	}

	FORCEINLINE TVoxelStaticArray& operator=(const TVoxelStaticArray& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			(*this)[Index] = Other[Index];
		}

		return *this;
	}
	FORCEINLINE TVoxelStaticArray& operator=(TVoxelStaticArray&& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			(*this)[Index] = MoveTemp(Other[Index]);
		}

		return *this;
	}
};

checkStatic(std::is_trivially_destructible_v<TVoxelStaticArray<int32, 1>>);

template<typename T, int32 Size, int32 Alignment>
struct TIsContiguousContainer<TVoxelStaticArray<T, Size, Alignment>>
{
	static constexpr bool Value = true;
};

template<typename T, int32 Size, int32 Alignment>
requires std::is_trivially_destructible_v<T>
struct TCanBulkSerialize<TVoxelStaticArray<T, Size, Alignment>>
{
	static constexpr bool Value = TCanBulkSerialize<T>::Value;
};