// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

FORCEINLINE uint64 operator ""_u64(const uint64 Value)
{
	return Value;
}

namespace FVoxelUtilities
{
	// Unlike DivideAndRoundDown, actually floors the result even if Dividend < 0
	template<typename DividendType, typename DivisorType>
	FORCEINLINE constexpr DividendType DivideFloor(DividendType Dividend, DivisorType Divisor)
	{
		checkStatic(std::is_same_v<DividendType, int32> || std::is_same_v<DividendType, int64>);
		checkStatic(std::is_same_v<DivisorType, int32> || std::is_same_v<DivisorType, int64>);
		checkStatic(std::is_same_v<DividendType, DivisorType> || std::is_same_v<DividendType, int64>);

		const DividendType Q = Dividend / Divisor;
		const DividendType R = Dividend % Divisor;
		return R ? (Q - ((Dividend < 0) ^ (Divisor < 0))) : Q;
	}
	template<typename T>
	FORCEINLINE T DivideFloor_Positive(T Dividend, T Divisor)
	{
		checkVoxelSlow(Dividend >= 0);
		checkVoxelSlow(Divisor > 0);
		return Dividend / Divisor;
	}
	// ~2x faster than DivideFloor
	FORCEINLINE int32 DivideFloor_FastLog2(const int32 Dividend, const int32 DivisorLog2)
	{
		checkVoxelSlow(DivisorLog2 >= 0);
		ensureVoxelSlow(DivisorLog2 < 32);
		constexpr int32 Offset = 1 << 30;
		checkVoxelSlow(Dividend < Offset);
		checkVoxelSlow(Dividend > -Offset);
		// Make the dividend positive to be able to use bit shifts
		const int32 Result = ((Dividend + Offset) >> DivisorLog2) - (Offset >> DivisorLog2);
		checkVoxelSlow(Result == DivideFloor(Dividend, 1 << DivisorLog2));
		return Result;
	}

	// Unlike DivideAndRoundUp, actually ceils the result even if Dividend < 0
	template<typename DividendType, typename DivisorType>
	FORCEINLINE constexpr DividendType DivideCeil(const DividendType Dividend, const DivisorType Divisor)
	{
		checkStatic(std::is_same_v<DividendType, int32> || std::is_same_v<DividendType, int64>);
		checkStatic(std::is_same_v<DivisorType, int32> || std::is_same_v<DivisorType, int64>);
		checkStatic(std::is_same_v<DividendType, DivisorType> || std::is_same_v<DividendType, int64>);

		return (Dividend > 0) ? 1 + (Dividend - 1) / Divisor : (Dividend / Divisor);
	}
	template<typename T>
	FORCEINLINE T DivideCeil_Positive(T Dividend, T Divisor)
	{
		checkVoxelSlow(Dividend >= 0);
		checkVoxelSlow(Divisor > 0);
		return (Dividend + Divisor - 1) / Divisor;
	}

	template<typename T>
	FORCEINLINE T PositiveMod(T Dividend, T Divisor)
	{
		return ((Dividend % Divisor) + Divisor) % Divisor;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<int32 Base, int32 InValue, typename = std::enable_if_t<(InValue > 0)>>
	FORCEINLINE constexpr int32 FloorLog()
	{
		int32 Value = InValue;

		int32 Exponent = -1;
		while (Value)
		{
			Value /= Base;
			Exponent++;
		}

		int64 Result = 1;
		for (int32 Index = 0; Index < Exponent; Index++)
		{
			Result *= Base;
		}

		if (InValue == Result)
		{
			return Exponent;
		}
		else
		{
			return Exponent - 1;
		}
	}
	template<int32 Base, int32 InValue, typename = std::enable_if_t<(InValue > 0)>>
	FORCEINLINE constexpr int32 CeilLog()
	{
		int32 Value = InValue;

		int32 Exponent = -1;
		while (Value)
		{
			Value /= Base;
			Exponent++;
		}

		int64 Result = 1;
		for (int32 Index = 0; Index < Exponent; Index++)
		{
			Result *= Base;
		}

		if (InValue == Result)
		{
			return Exponent;
		}
		else
		{
			return Exponent + 1;
		}
	}
	template<int32 Base, int32 InValue, typename = std::enable_if_t<(InValue > 0) && FloorLog<Base, InValue>() == CeilLog<Base, InValue>()>>
	FORCEINLINE constexpr int32 ExactLog()
	{
		int32 Value = InValue;
		int32 Exponent = -1;
		while (Value)
		{
			Value /= Base;
			Exponent++;
		}
		return Exponent;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<int32 Value, typename = std::enable_if_t<(Value > 0)>>
	constexpr int32 FloorLog2() { return FloorLog<2, Value>(); }

	template<int32 Value, typename = std::enable_if_t<(Value > 0)>>
	constexpr int32 CeilLog2() { return CeilLog<2, Value>(); }

	template<int32 Value, typename = std::enable_if_t<(Value > 0) && FloorLog2<Value>() == CeilLog2<Value>()>>
	constexpr int32 ExactLog2() { return ExactLog<2, Value>(); }

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<int32 Value, typename = std::enable_if_t<(Value > 0)>>
	constexpr int32 FloorLog10() { return FloorLog<10, Value>(); }

	template<int32 Value, typename = std::enable_if_t<(Value > 0)>>
	constexpr int32 CeilLog10() { return CeilLog<10, Value>(); }

	template<int32 Value, typename = std::enable_if_t<(Value > 0) && FloorLog10<Value>() == CeilLog10<Value>()>>
	constexpr int32 ExactLog10() { return ExactLog<10, Value>(); }

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE int32 ExactLog2(const int32 X)
	{
		const int32 Log2 = FMath::FloorLog2(X);
		checkVoxelSlow((1 << Log2) == X);
		return Log2;
	}
	FORCEINLINE int64 IntPow(const int64 Value, const int64 Exponent)
	{
		checkVoxelSlow(Exponent >= 0);
		ensureVoxelSlow(Exponent < 1024);

		int64 Result = 1;
		for (int32 Index = 0; Index < Exponent; Index++)
		{
			Result *= Value;
		}
		return Result;
	}

	template<int32 NumPerChunk>
	FORCEINLINE int32 GetChunkIndex(const int32 Index)
	{
		// Make sure to not use unsigned division as it would add a sign check
		checkVoxelSlow(Index >= 0);
		constexpr int32 Log2 = ExactLog2<NumPerChunk>();
		return Index >> Log2;
	}
	template<int32 NumPerChunk>
	FORCEINLINE int32 GetChunkOffset(const int32 Index)
	{
		checkVoxelSlow(Index >= 0);
		checkStatic(FMath::IsPowerOfTwo(NumPerChunk));
		return Index & (NumPerChunk - 1);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE constexpr uint8 ClampToUINT8(const int32 Value)
	{
		return FMath::Clamp<int32>(Value, 0, MAX_uint8);
	}
	FORCEINLINE constexpr uint16 ClampToUINT16(const int32 Value)
	{
		return FMath::Clamp<int32>(Value, 0, MAX_uint16);
	}

	FORCEINLINE uint8 FloatToUINT8(const float Float)
	{
		return ClampToUINT8(FMath::FloorToInt(Float * 255.999f));
	}
	FORCEINLINE constexpr float UINT8ToFloat(const uint8 Int)
	{
		return Int / 255.f;
	}

	FORCEINLINE uint16 FloatToUINT16(const float Float)
	{
		return ClampToUINT16(FMath::FloorToInt(Float * 65535.999f));
	}
	FORCEINLINE constexpr float UINT16ToFloat(const uint16 Int)
	{
		return Int / 65535.f;
	}

	FORCEINLINE bool IsValidINT8(const int32 Value)
	{
		return MIN_int8 <= Value && Value <= MAX_int8;
	}
	FORCEINLINE bool IsValidINT16(const int32 Value)
	{
		return MIN_int16 <= Value && Value <= MAX_int16;
	}

	FORCEINLINE bool IsValidUINT8(const int32 Value)
	{
		return 0 <= Value && Value <= MAX_uint8;
	}
	FORCEINLINE bool IsValidUINT16(const int32 Value)
	{
		return 0 <= Value && Value <= MAX_uint16;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE uint32 ReadBits(uint32 Data, const int32 FirstBit, const int32 NumBits)
	{
		checkVoxelSlow(0 <= FirstBit && FirstBit + NumBits <= 32);
		checkVoxelSlow(0 < NumBits);

		Data <<= 32 - (FirstBit + NumBits);
		Data >>= 32 - NumBits;
		return Data;
	}
	FORCEINLINE uint64 ReadBits(uint64 Data, const int32 FirstBit, const int32 NumBits)
	{
		checkVoxelSlow(0 <= FirstBit && FirstBit + NumBits <= 64);
		checkVoxelSlow(0 < NumBits);

		Data <<= 64 - (FirstBit + NumBits);
		Data >>= 64 - NumBits;
		return Data;
	}
	template<typename A, typename B, typename C>
	void ReadBits(A, B, C) = delete;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE float& FloatBits(uint32& Value)
	{
		return reinterpret_cast<float&>(Value);
	}
	FORCEINLINE double& FloatBits(uint64& Value)
	{
		return reinterpret_cast<double&>(Value);
	}

	FORCEINLINE uint32& IntBits(float& Value)
	{
		return reinterpret_cast<uint32&>(Value);
	}
	FORCEINLINE uint64& IntBits(double& Value)
	{
		return reinterpret_cast<uint64&>(Value);
	}

	FORCEINLINE const float& FloatBits(const uint32& Value)
	{
		return reinterpret_cast<const float&>(Value);
	}
	FORCEINLINE const double& FloatBits(const uint64& Value)
	{
		return reinterpret_cast<const double&>(Value);
	}

	FORCEINLINE const uint32& IntBits(const float& Value)
	{
		return reinterpret_cast<const uint32&>(Value);
	}
	FORCEINLINE const uint64& IntBits(const double& Value)
	{
		return reinterpret_cast<const uint64&>(Value);
	}

	template<typename T>
	void FloatBits(T) = delete;
	template<typename T>
	void IntBits(T) = delete;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE float NaNf()
	{
		return FloatBits(0xFFFFFFFF);
	}
	FORCEINLINE double NaNd()
	{
		return FloatBits(0xFFFFFFFFFFFFFFFF_u64);
	}

	FORCEINLINE bool IsNaN(const float Value)
	{
		const bool bIsNaN = (IntBits(Value) & 0x7FFFFFFF) > 0x7F800000;
		checkVoxelSlow(bIsNaN == FMath::IsNaN(Value));
		return bIsNaN;
	}
	FORCEINLINE bool IsNaN(const double Value)
	{
		const bool bIsNaN = (IntBits(Value) & 0x7FFFFFFFFFFFFFFF_u64) > 0x7FF0000000000000_u64;
		checkVoxelSlow(bIsNaN == FMath::IsNaN(Value));
		return bIsNaN;
	}

	template<typename T>
	bool IsNaN(T) = delete;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE bool SignBit(const float Value)
	{
		return IntBits(Value) & (1u << 31);
	}
	template<typename T>
	bool SignBit(const T&) = delete;

	FORCEINLINE int32 GetExponent(const float Value)
	{
		return ReadBits(IntBits(Value), 23, 8);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE uint32 CountBits(const uint32 Bits)
	{
#if PLATFORM_WINDOWS && !PLATFORM_COMPILER_CLANG
		// Force use the intrinsic
		return _mm_popcnt_u32(Bits);
#else
		return FMath::CountBits(Bits);
#endif
	}
	FORCEINLINE uint32 CountBits(const uint64 Bits)
	{
#if PLATFORM_WINDOWS && !PLATFORM_COMPILER_CLANG
		// Force use the intrinsic
		return _mm_popcnt_u64(Bits);
#else
		return FMath::CountBits(Bits);
#endif
	}

	FORCEINLINE uint32 FirstBitLow(const uint32 Bits)
	{
#if PLATFORM_WINDOWS && !PLATFORM_COMPILER_CLANG
		return _tzcnt_u32(Bits);
#else
		return __tzcnt_u32(Bits);
#endif
	}
	FORCEINLINE uint32 FirstBitLow(const uint64 Bits)
	{
#if PLATFORM_WINDOWS && !PLATFORM_COMPILER_CLANG
		return _tzcnt_u64(Bits);
#else
		return __tzcnt_u64(Bits);
#endif
	}
}