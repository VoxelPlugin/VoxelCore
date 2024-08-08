// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"

template<typename T, int32 Size, int32 Alignment = alignof(T), bool bForceInitToZero = false>
class alignas(Alignment) TVoxelStaticArray
{
public:
	using ElementType = T;

	checkStatic(!bForceInitToZero || TIsTriviallyDestructible<T>::Value);
	static constexpr bool bCanNoInit = TIsTriviallyDestructible<T>::Value && !bForceInitToZero;

	// Default constructor, only valid for complex types
	// For trivially destructible types you need to choose whether to init them (ForceInit) or not (NoInit)
	INTELLISENSE_ONLY(template<typename = std::enable_if_t<!bCanNoInit>>)
	TVoxelStaticArray()
	{
		checkStatic(!bCanNoInit);

		for (T& Element : *this)
		{
			new (&Element) T{};
		}
	}
	FORCEINLINE explicit TVoxelStaticArray(EForceInit)
	{
		for (T& Element : *this)
		{
			new (&Element) T{};
		}
	}
	INTELLISENSE_ONLY(template<typename = std::enable_if_t<bCanNoInit>>)
	FORCEINLINE explicit TVoxelStaticArray(ENoInit)
	{
		checkStatic(bCanNoInit);

#if VOXEL_DEBUG
		for (uint8& Byte : Data)
		{
			Byte = 0xDE;
		}
#endif
	}

	FORCEINLINE explicit TVoxelStaticArray(T Value)
	{
		for (T& Element : *this)
		{
			new (&Element) T(Value);
		}
	}
	template<typename... TArgs, typename = std::enable_if_t<sizeof...(TArgs) == Size && Size != 1>>
	FORCEINLINE TVoxelStaticArray(TArgs... Args)
	{
		static_assert(sizeof...(Args) == Size, "");
		SetFromVariadicArgs(Args...);
	}
	FORCEINLINE TVoxelStaticArray(const TVoxelStaticArray& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (GetData() + Index) T(Other[Index]);
		}
	}
	FORCEINLINE TVoxelStaticArray(TVoxelStaticArray&& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			new (GetData() + Index) T(MoveTemp(Other[Index]));
		}
	}
	FORCEINLINE ~TVoxelStaticArray()
	{
		if (!TIsTriviallyDestructible<T>::Value)
		{
			for (auto& Element : *this)
			{
				Element.~T();
			}
		}
	}

	FORCEINLINE TVoxelStaticArray& operator=(const TVoxelStaticArray& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			GetData()[Index] = Other[Index];
		}

		return *this;
	}
	FORCEINLINE TVoxelStaticArray& operator=(TVoxelStaticArray&& Other)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			GetData()[Index] = MoveTemp(Other[Index]);
		}

		return *this;
	}

	FORCEINLINE TVoxelStaticArray& operator=(const T Value)
	{
		// Always safe even with non-pod
		for (int32 Index = 0; Index < Size; Index++)
		{
			GetData()[Index] = Value;
		}

		return *this;
	}

	FORCEINLINE static constexpr int32 Num()
	{
		return Size;
	}
	FORCEINLINE static constexpr int32 GetTypeSize()
	{
		return sizeof(T);
	}

	FORCEINLINE bool IsValidIndex(const int32 Index) const
	{
		return 0 <= Index && Index < Num();
	}

	FORCEINLINE void Memzero()
	{
		FMemory::Memzero(GetData(), Size * sizeof(T));
	}
	FORCEINLINE void Memset(const uint8 Value)
	{
		FMemory::Memset(GetData(), Value, Size * sizeof(T));
	}

	FORCEINLINE T* GetData()
	{
		return reinterpret_cast<T*>(Data);
	}
	FORCEINLINE const T* GetData() const
	{
		return reinterpret_cast<const T*>(Data);
	}

	template<typename TFrom, typename Allocator>
	void CopyFromArray(const TArray<TFrom, Allocator>& Array, const bool bInitializeEnd = true)
	{
		check(Size >= Array.Num());
		for (int32 Index = 0; Index < Array.Num(); Index++)
		{
			(*this)[Index] = Array[Index];
		}
		if (bInitializeEnd)
		{
			for (int32 Index = Array.Num(); Index < Size; Index++)
			{
				(*this)[Index] = T{};
			}
		}
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

	operator TArray<T>() const
	{
		return TArray<T>(GetData(), Num());
	}

	operator TVoxelArrayView<T>()
	{
		return TVoxelArrayView<T>(GetData(), Num());
	}
	operator TVoxelArrayView<const T>() const
	{
		return TVoxelArrayView<const T>(GetData(), Num());
	}

	FORCEINLINE T* begin() { return GetData(); }
	FORCEINLINE T* end()   { return GetData() + Size; }

	FORCEINLINE const T* begin() const { return GetData(); }
	FORCEINLINE const T* end()   const { return GetData() + Size; }

	template<int32 Index = 0, typename... TArgs>
	FORCEINLINE void SetFromVariadicArgs(T Arg, TArgs... Args)
	{
		static_assert(0 <= Index && Index < Size, "");
		static_assert(sizeof...(Args) == Size - 1 - Index, "");
		(*this)[Index] = Arg;
		SetFromVariadicArgs<Index + 1>(Args...);
	}
	template<int32 Index = 0>
	FORCEINLINE void SetFromVariadicArgs(T Arg)
	{
		static_assert(Index == Size - 1, "");
		(*this)[Index] = Arg;
	}

	friend FArchive& operator<<(FArchive& Ar, TVoxelStaticArray& Array)
	{
		static_assert(TIsTriviallyDestructible<T>::Value, "Need to fix serializer");
		Ar.Serialize(Array.GetData(), Array.Num() * Array.GetTypeSize());
		return Ar;
	}
	bool operator==(const TVoxelStaticArray& Other) const
	{
		return CompareItems(GetData(), Other.GetData(), Num());
	}

private:
	uint8 Data[Size * sizeof(T)];
};

template<typename T, int32 Size, int32 Alignment = alignof(T)>
using TVoxelStaticArray_ForceInit = TVoxelStaticArray<T, Size, Alignment, true>;

template<typename T, int32 Size, int32 Alignment, bool bForceInitToZero>
struct TIsContiguousContainer<TVoxelStaticArray<T, Size, Alignment, bForceInitToZero>>
{
	static constexpr bool Value = true;
};

template<typename T, int32 Size, int32 Alignment, bool bForceInitToZero>
struct TIsTriviallyDestructible<TVoxelStaticArray<T, Size, Alignment, bForceInitToZero>>
{
	static constexpr bool Value = TIsTriviallyDestructible<T>::Value;
};

template<typename T, int32 Size, int32 Alignment, bool bForceInitToZero>
struct TCanBulkSerialize<TVoxelStaticArray<T, Size, Alignment, bForceInitToZero>>
{
	static constexpr bool Value = TVoxelCanBulkSerialize<T>::Value;
};