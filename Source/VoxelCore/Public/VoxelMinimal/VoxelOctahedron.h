// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelMathUtilities.h"
#include "VoxelOctahedron.generated.h"

namespace FVoxelUtilities
{
	FORCEINLINE FVector2f UnitVectorToOctahedron(FVector3f Unit)
	{
		ensureVoxelSlowNoSideEffects(Unit.IsNormalized());

		const float AbsSum = FMath::Abs(Unit.X) + FMath::Abs(Unit.Y) + FMath::Abs(Unit.Z);
		Unit.X /= AbsSum;
		Unit.Y /= AbsSum;

		FVector2f Result = FVector2f(Unit.X, Unit.Y);
		if (Unit.Z <= 0)
		{
			Result.X = (1 - FMath::Abs(Unit.Y)) * (Unit.X >= 0 ? 1 : -1);
			Result.Y = (1 - FMath::Abs(Unit.X)) * (Unit.Y >= 0 ? 1 : -1);
		}
		return Result * 0.5f + 0.5f;
	}
	FORCEINLINE FVector3f OctahedronToUnitVector(FVector2f Octahedron)
	{
		ensureVoxelSlow(0 <= Octahedron.X && Octahedron.X <= 1);
		ensureVoxelSlow(0 <= Octahedron.Y && Octahedron.Y <= 1);

		Octahedron = Octahedron * 2.f - 1.f;

		FVector3f Unit;
		Unit.X = Octahedron.X;
		Unit.Y = Octahedron.Y;
		Unit.Z = 1.f - FMath::Abs(Octahedron.X) - FMath::Abs(Octahedron.Y);

		const float T = FMath::Max(-Unit.Z, 0.f);

		Unit.X += Unit.X >= 0 ? -T : T;
		Unit.Y += Unit.Y >= 0 ? -T : T;

		ensureVoxelSlowNoSideEffects(Unit.SizeSquared() >= KINDA_SMALL_NUMBER);

		return Unit.GetUnsafeNormal();
	}
}

#define __ISPC_STRUCT_FVoxelOctahedron__

namespace ispc
{
	struct FVoxelOctahedron
	{
		uint8 X = 0;
		uint8 Y = 0;
	};
}

USTRUCT(DisplayName = "Voxel Normal")
struct alignas(2) FVoxelOctahedron
#if CPP
	: ispc::FVoxelOctahedron
#endif
{
	GENERATED_BODY()

public:
#if !CPP
	UPROPERTY()
	uint8 X = 0;

	UPROPERTY()
	uint8 Y = 0;
#endif

public:
	FVoxelOctahedron() = default;
	FORCEINLINE explicit FVoxelOctahedron(const FVector2f& Octahedron)
	{
		X = FVoxelUtilities::FloatToUINT8(Octahedron.X);
		Y = FVoxelUtilities::FloatToUINT8(Octahedron.Y);

		ensureVoxelSlow(0 <= Octahedron.X && Octahedron.X <= 1);
		ensureVoxelSlow(0 <= Octahedron.Y && Octahedron.Y <= 1);
	}
	FORCEINLINE explicit FVoxelOctahedron(const FVector3f& UnitVector)
		: FVoxelOctahedron(FVoxelUtilities::UnitVectorToOctahedron(UnitVector))
	{
	}

public:
	FORCEINLINE FVector2f GetOctahedron() const
	{
		return
		{
				FVoxelUtilities::UINT8ToFloat(X),
				FVoxelUtilities::UINT8ToFloat(Y)
		};
	}
	FORCEINLINE FVector3f GetUnitVector() const
	{
		return FVoxelUtilities::OctahedronToUnitVector(GetOctahedron());
	}

public:
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FVoxelOctahedron& Octahedron)
	{
		Ar << Octahedron.X;
		Ar << Octahedron.Y;
		return Ar;
	}
};
checkStatic(sizeof(FVoxelOctahedron) == 2);