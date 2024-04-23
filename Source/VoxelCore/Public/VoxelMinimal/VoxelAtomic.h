// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include <atomic>

enum class EVoxelAtomicType
{
	None,
	PositiveInteger
};

// Copyable atomic
template<typename T, EVoxelAtomicType Type = EVoxelAtomicType::None>
struct TVoxelAtomic
{
public:
	TVoxelAtomic()
	{
		Atomic = T{};
	}
	TVoxelAtomic(const T Value)
		: Atomic(Value)
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
		return Atomic.load(MemoryOrder);
	}
	FORCEINLINE void Set(const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		Atomic.store(NewValue, MemoryOrder);
	}

	FORCEINLINE T Exchange_ReturnOld(const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Atomic.exchange(NewValue, MemoryOrder);
	}
	// Return true if exchange was successful
	// Expected will hold the old value (only useful if fails, if succeeds Expected isn't changed)
	FORCEINLINE bool CompareExchangeStrong(T& Expected, const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Atomic.compare_exchange_strong(Expected, NewValue, MemoryOrder);
	}
	// Might spuriously fail
	FORCEINLINE bool CompareExchangeWeak(T& Expected, const T NewValue, const std::memory_order MemoryOrder = std::memory_order_seq_cst)
	{
		CheckValue(NewValue);
		return Atomic.compare_exchange_weak(Expected, NewValue, MemoryOrder);
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
		if constexpr (
			std::is_integral_v<T> ||
			(__cplusplus >= 202002L && std::is_floating_point_v<T>))
		{
			const T OldValue = Atomic.fetch_add(Operand, MemoryOrder);
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
	T& AsNonAtomic()
	{
		return ReinterpretCastRef<T>(*this);
	}
	const T& AsNonAtomic() const
	{
		return ReinterpretCastRef<T>(*this);
	}

private:
	std::atomic<T> Atomic;

	FORCEINLINE static void CheckValue(const T Value)
	{
		if constexpr (Type == EVoxelAtomicType::PositiveInteger)
		{
			ensureVoxelSlow(Value >= 0);
		}
	}

	checkStatic(std::atomic<T>::is_always_lock_free);
};

using FVoxelCounter32 = TVoxelAtomic<int32, EVoxelAtomicType::PositiveInteger>;
using FVoxelCounter64 = TVoxelAtomic<int64, EVoxelAtomicType::PositiveInteger>;