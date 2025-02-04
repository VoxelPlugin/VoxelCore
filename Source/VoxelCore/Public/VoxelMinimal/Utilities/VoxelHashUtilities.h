// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Containers/VoxelArrayView.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"

namespace FVoxelUtilities
{
	FORCEINLINE constexpr uint32 MurmurHash32(uint32 Hash)
	{
		Hash ^= Hash >> 16;
		Hash *= 0x85ebca6b;
		Hash ^= Hash >> 13;
		Hash *= 0xc2b2ae35;
		Hash ^= Hash >> 16;
		return Hash;
	}
	FORCEINLINE constexpr uint64 MurmurHash64(uint64 Hash)
	{
		Hash ^= Hash >> 33;
		Hash *= 0xff51afd7ed558ccd;
		Hash ^= Hash >> 33;
		Hash *= 0xc4ceb9fe1a85ec53;
		Hash ^= Hash >> 33;
		return Hash;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE uint32 MurmurHash32xN(uint32 const* RESTRICT Hash, const int32 Size, const uint32 Seed = 0)
	{
		uint32 H = 1831214719 * (1460481823 + Seed);
		for (int32 Index = 0; Index < Size; Index++)
		{
			uint32 K = Hash[Index];
			K *= 0xcc9e2d51;
			K = (K << 15) | (K >> 17);
			K *= 0x1b873593;
			H ^= K;
			H = (H << 13) | (H >> 19);
			H = H * 5 + 0xe6546b64;
		}

		H ^= uint32(Size);
		H ^= H >> 16;
		H *= 0x85ebca6b;
		H ^= H >> 13;
		H *= 0xc2b2ae35;
		H ^= H >> 16;
		return H;
	}

	template<typename T, typename = std::enable_if_t<
		std::is_trivially_destructible_v<T> &&
		!TIsTArrayView<T>::Value && (
		sizeof(T) == sizeof(uint8) ||
		sizeof(T) == sizeof(uint16) ||
		sizeof(T) % sizeof(uint32) == 0)>>
	FORCEINLINE uint64 MurmurHash(const T& Value, const uint32 Seed = 0)
	{
		uint32 H = 1831214719 * (1460481823 + Seed);

		if constexpr (std::is_same_v<T, bool>)
		{
			return MurmurHash64(H ^ uint64(Value ? 3732917 : 2654653));
		}
		if constexpr (sizeof(T) == sizeof(uint8))
		{
			return MurmurHash64(H ^ uint64(ReinterpretCastRef<uint8>(Value)));
		}
		if constexpr (sizeof(T) == sizeof(uint16))
		{
			return MurmurHash64(H ^ uint64(ReinterpretCastRef<uint16>(Value)));
		}
		if constexpr (sizeof(T) == sizeof(uint32))
		{
			return MurmurHash64(H ^ uint64(ReinterpretCastRef<uint32>(Value)));
		}
		if constexpr (sizeof(T) == sizeof(uint64))
		{
			if constexpr (alignof(T) >= alignof(uint64))
			{
				return MurmurHash64(H ^ uint64(ReinterpretCastRef<uint64>(Value)));
			}
			else
			{
				return MurmurHash64(H ^ uint64(ReinterpretCastRef_Unaligned<uint64>(Value)));
			}
		}

		constexpr int32 Size = sizeof(T) / sizeof(uint32);
		const uint32* RESTRICT Hash = reinterpret_cast<const uint32*>(&Value);

		for (int32 Index = 0; Index < Size; Index++)
		{
			uint32 K = Hash[Index];
			K *= 0xcc9e2d51;
			K = (K << 15) | (K >> 17);
			K *= 0x1b873593;
			H ^= K;
			H = (H << 13) | (H >> 19);
			H = H * 5 + 0xe6546b64;
		}
		return MurmurHash64(H ^ uint32(Size));
	}

	FORCEINLINE uint64 MurmurHashBytes(const TConstVoxelArrayView<uint8>& Bytes, const uint32 Seed = 0)
	{
		constexpr int32 WordSize = sizeof(uint32);
		const int32 Size = Bytes.Num() / WordSize;
		const uint32* RESTRICT Hash = reinterpret_cast<const uint32*>(Bytes.GetData());

		uint32 H = 1831214719 * (1460481823 + Seed);
		for (int32 Index = 0; Index < Size; Index++)
		{
			uint32 K = Hash[Index];
			K *= 0xcc9e2d51;
			K = (K << 15) | (K >> 17);
			K *= 0x1b873593;
			H ^= K;
			H = (H << 13) | (H >> 19);
			H = H * 5 + 0xe6546b64;
		}

		uint32 Tail = 0;
		for (int32 Index = Size * WordSize; Index < Bytes.Num(); Index++)
		{
			Tail = (Tail << 8) | Bytes[Index];
		}

		return MurmurHash64(H ^ Tail ^ uint32(Size));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE uint64 MurmurHashMultiImpl(uint32 Seed, const T& Arg)
	{
		return FVoxelUtilities::MurmurHash(Arg, Seed);
	}
	template<typename T, typename... ArgTypes>
	FORCEINLINE uint64 MurmurHashMultiImpl(uint32 Seed, const T& Arg, const ArgTypes&... Args)
	{
		return FVoxelUtilities::MurmurHash(Arg, Seed) ^ FVoxelUtilities::MurmurHashMultiImpl(Seed + 1, Args...);
	}
	template<typename... ArgTypes>
	FORCEINLINE uint64 MurmurHashMulti(const ArgTypes&... Args)
	{
		return FVoxelUtilities::MurmurHashMultiImpl(0, Args...);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	template<typename T>
	FORCEINLINE uint32 HashValue(const T& Value)
	{
		if constexpr (std::is_same_v<T, FGuid>)
		{
			struct FRawGuid
			{
				uint64 A;
				uint64 B;
			};

			return
				FVoxelUtilities::MurmurHash64(ReinterpretCastRef_Unaligned<FRawGuid>(Value).A) ^
				FVoxelUtilities::MurmurHash64(ReinterpretCastRef_Unaligned<FRawGuid>(Value).B);
		}

		if constexpr (std::is_same_v<T, FIntPoint>)
		{
			const uint64 Hash = uint64(Value.X) | (uint64(Value.Y) << 32);
			return MurmurHash64(Hash);
		}

		if constexpr (std::is_same_v<T, FIntVector>)
		{
			uint64 Hash = uint64(Value.X) | (uint64(Value.Y) << 32);
			Hash ^= uint64(Value.Z) << 16;
			return MurmurHash64(Hash);
		}

		return GetTypeHash(Value);
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	FORCEINLINE int32 GetHashTableSize(const int32 NumElements)
	{
		// See TSetAllocator

		if (NumElements < 4)
		{
			return 1;
		}

		return 1 << FMath::CeilLogTwo(8 + NumElements / 2);
	}
	template<int32 NumElements>
	FORCEINLINE constexpr int32 GetHashTableSize()
	{
		if (NumElements < 4)
		{
			return 1;
		}

		return 1 << CeilLog2<8 + NumElements / 2>();
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FGuid CombineGuids(TConstVoxelArrayView<FGuid> Guids);

	template<typename... ArgTypes>
	FORCEINLINE FGuid CombineGuids(ArgTypes... Args)
	{
		return CombineGuids(TConstVoxelArrayView<FGuid>({ Args... }));
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// Returns in [0, 1)
	FORCEINLINE float GetFraction(const uint32 Seed)
	{
		const float Result = FloatBits(0x3F800000U | (Seed >> 9)) - 1.0f;
		checkVoxelSlow(0.f <= Result && Result < 1.f);
		return Result;
	}
	// Returns in [Min, Max]
	FORCEINLINE int32 RandRange(const uint32 Seed, const int32 Min, const int32 Max)
	{
		ensureVoxelSlow(Min <= Max);
		const int32 Result = Min + FMath::FloorToInt(GetFraction(Seed) * float((Max - Min) + 1));
		ensureVoxelSlow(Min <= Result && Result <= Max);
		return Result;
	}
	FORCEINLINE float RandRange(const uint32 Seed, const float Min, const float Max)
	{
		ensureVoxelSlow(Min <= Max);
		const float Result = Min + GetFraction(Seed) * (Max - Min);
		ensureVoxelSlow(Min <= Result && Result <= Max);
		return Result;
	}

	template<uint32 Base>
	FORCEINLINE float Halton(uint32 Index)
	{
		float Result = 0.0f;
		const float InvBase = 1.0f / Base;
		float Fraction = InvBase;
		while (Index > 0)
		{
			Result += (Index % Base) * Fraction;
			Index /= Base;
			Fraction *= InvBase;
		}
		return Result;
	}

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	VOXELCORE_API FSHAHash ShaHash(TConstVoxelArrayView64<uint8> Data);
}