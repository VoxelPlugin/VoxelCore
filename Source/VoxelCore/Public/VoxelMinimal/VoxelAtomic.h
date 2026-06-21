// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelArrayUtilities.h"
#include <atomic>

enum class EVoxelAtomicType
{
	None,
	PositiveInteger
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

enum class EVoxelAtomicPadding
{
	Enabled,
	Disabled
};

template<typename, EVoxelAtomicPadding>
struct TVoxelAtomicStorage;

template<typename T>
struct TVoxelAtomicStorage<T, EVoxelAtomicPadding::Enabled>
{
	VOXEL_ATOMIC_PADDING;
	std::atomic<T> Atomic;
	VOXEL_ATOMIC_PADDING;

	FORCEINLINE TVoxelAtomicStorage()
	{
		Atomic = T{};
	}
	FORCEINLINE TVoxelAtomicStorage(const T Value)
		: Atomic(Value)
	{
	}
};

template<typename T>
struct TVoxelAtomicStorage<T, EVoxelAtomicPadding::Disabled>
{
	std::atomic<T> Atomic;

	FORCEINLINE TVoxelAtomicStorage()
	{
		Atomic = T{};
	}
	FORCEINLINE TVoxelAtomicStorage(const T Value)
		: Atomic(Value)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Copyable atomic
template<
	typename T,
	EVoxelAtomicPadding Padding = EVoxelAtomicPadding::Disabled,
	EVoxelAtomicType Type = EVoxelAtomicType::None>
struct TVoxelAtomic
{
public:
	TVoxelAtomic() = default;
	TVoxelAtomic(const T Value)
		: Storage(Value)
	{
	}

	FORCEINLINE TVoxelAtomic(const TVoxelAtomic& Other)
	{
		this->Set(Other.Get());
	}
	FORCEINLINE TVoxelAtomic(TVoxelAtomic&& Other)
	{
		this->Set(Other.Get());
	}

public:
	FORCEINLINE TVoxelAtomic& operator=(const TVoxelAtomic& Other)
	{
		this->Set(Other.Get());
		return *this;
	}
	FORCEINLINE TVoxelAtomic& operator=(TVoxelAtomic&& Other)
	{
		this->Set(Other.Get());
		return *this;
	}

	template<typename Other>
	FORCEINLINE TVoxelAtomic& operator=(Other) = delete;

public:
	FORCEINLINE T Get(const std::memory_order MemoryOrder = std::memory_order_seq_cst) const
	{
		return Storage.Atomic.load(MemoryOrder);
	}
	FORCEINLINE void Set(const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		Storage.Atomic.store(NewValue, MemoryOrder);
	}

	FORCEINLINE T Set_ReturnOld(const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Storage.Atomic.exchange(NewValue, MemoryOrder);
	}
	// Return true if exchange was successful
	// Expected will hold the old value (only useful if fails, if succeeds Expected isn't changed)
	FORCEINLINE bool CompareExchangeStrong(T& Expected, const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Storage.Atomic.compare_exchange_strong(Expected, NewValue, MemoryOrder);
	}
	// Might spuriously fail
	FORCEINLINE bool CompareExchangeWeak(T& Expected, const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Storage.Atomic.compare_exchange_weak(Expected, NewValue, MemoryOrder);
	}

	template<typename LambdaType>
	FORCEINLINE T Apply_ReturnOld(const LambdaType Lambda, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		T OldValue = Get(std::memory_order_relaxed);

		while (true)
		{
			const T NewValue = Lambda(static_cast<const T&>(OldValue));

			const T OldValueCopy = OldValue;
			if (this->CompareExchangeStrong(OldValue, NewValue, MemoryOrder))
			{
				checkVoxelSlow(FVoxelUtilities::Equal(MakeByteVoxelArrayView(OldValue), MakeByteVoxelArrayView(OldValueCopy)));
				return OldValue;
			}
		}
	}
	template<typename LambdaType>
	FORCEINLINE T Apply_ReturnNew(const LambdaType Lambda, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		T OldValue = Get(std::memory_order_relaxed);

		while (true)
		{
			const T NewValue = Lambda(static_cast<const T&>(OldValue));

			if (this->CompareExchangeStrong(OldValue, NewValue, MemoryOrder))
			{
				return NewValue;
			}
		}
	}
	template<typename LambdaType>
	FORCEINLINE void Apply(const LambdaType Lambda, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Apply_ReturnOld(Lambda, MemoryOrder);
	}

public:
	FORCEINLINE T Add_ReturnOld(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		if constexpr (std::is_integral_v<T>)
		{
			const T OldValue = Storage.Atomic.fetch_add(Operand, MemoryOrder);
			CheckValue(OldValue + Operand);
			return OldValue;
		}
		else
		{
			return this->Apply_ReturnOld([&](const T OldValue)
			{
				CheckValue(OldValue + Operand);
				return OldValue + Operand;
			});
		}
	}
	FORCEINLINE T Add_ReturnNew(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return this->Add_ReturnOld(Operand, MemoryOrder) + Operand;
	}
	FORCEINLINE void Add(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Add_ReturnOld(Operand, MemoryOrder);
	}

public:
	FORCEINLINE T Subtract_ReturnOld(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return this->Add_ReturnOld(-Operand, MemoryOrder);
	}
	FORCEINLINE T Subtract_ReturnNew(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return this->Add_ReturnNew(-Operand, MemoryOrder);
	}
	FORCEINLINE void Subtract(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Add_ReturnOld(-Operand, MemoryOrder);
	}

public:
	FORCEINLINE T Increment_ReturnOld(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return Add_ReturnOld(1, MemoryOrder);
	}
	FORCEINLINE T Increment_ReturnNew(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return Add_ReturnNew(1, MemoryOrder);
	}
	FORCEINLINE void Increment(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Increment_ReturnOld(MemoryOrder);
	}

public:
	FORCEINLINE T Decrement_ReturnOld(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return Add_ReturnOld(-1, MemoryOrder);
	}
	FORCEINLINE T Decrement_ReturnNew(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return Add_ReturnNew(-1, MemoryOrder);
	}
	FORCEINLINE void Decrement(const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Decrement_ReturnOld(MemoryOrder);
	}

public:
	FORCEINLINE T Or_ReturnOld(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		checkStatic(std::is_integral_v<T>);
		const T OldValue = Storage.Atomic.fetch_or(Operand, MemoryOrder);
		CheckValue(OldValue | Operand);
		return OldValue;
	}
	FORCEINLINE T Or_ReturnNew(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return this->Or_ReturnOld(Operand, MemoryOrder) | Operand;
	}
	FORCEINLINE void Or(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->Or_ReturnOld(Operand, MemoryOrder);
	}

public:
	FORCEINLINE T And_ReturnOld(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		checkStatic(std::is_integral_v<T>);
		const T OldValue = Storage.Atomic.fetch_and(Operand, MemoryOrder);
		CheckValue(OldValue & Operand);
		return OldValue;
	}
	FORCEINLINE T And_ReturnNew(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		return this->And_ReturnOld(Operand, MemoryOrder) & Operand;
	}
	FORCEINLINE void And(const T Operand, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		this->And_ReturnOld(Operand, MemoryOrder);
	}

public:
	T& AsNonAtomic()
	{
		return ReinterpretCastRef<T>(Storage.Atomic);
	}
	const T& AsNonAtomic() const
	{
		return ReinterpretCastRef<T>(Storage.Atomic);
	}

	operator TVoxelAtomic<T>&()
	{
		return ReinterpretCastRef<TVoxelAtomic<T>>(Storage.Atomic);
	}
	operator const TVoxelAtomic<T>&() const
	{
		return ReinterpretCastRef<TVoxelAtomic<T>>(Storage.Atomic);
	}

private:
	TVoxelAtomicStorage<T, Padding> Storage;

	FORCEINLINE static void CheckValue(const T Value)
	{
		if constexpr (Type == EVoxelAtomicType::PositiveInteger)
		{
			ensureVoxelSlow(Value >= 0);
		}
	}

	checkStatic(std::atomic<T>::is_always_lock_free);
};

template<
	typename T,
	EVoxelAtomicType Type = EVoxelAtomicType::None>
using TVoxelAtomic_WithPadding = TVoxelAtomic<T, EVoxelAtomicPadding::Enabled, Type>;

using FVoxelCounter32 = TVoxelAtomic<int32, EVoxelAtomicPadding::Disabled, EVoxelAtomicType::PositiveInteger>;
using FVoxelCounter64 = TVoxelAtomic<int64, EVoxelAtomicPadding::Disabled, EVoxelAtomicType::PositiveInteger>;

using FVoxelCounter32_WithPadding = TVoxelAtomic_WithPadding<int32, EVoxelAtomicType::PositiveInteger>;
using FVoxelCounter64_WithPadding = TVoxelAtomic_WithPadding<int64, EVoxelAtomicType::PositiveInteger>;

// Declare here, cannot use FVoxelCounter64 in VoxelStats.h
VOXELCORE_API void RegisterVoxelInstanceCounter(FName StatName, const FVoxelCounter64& Counter);