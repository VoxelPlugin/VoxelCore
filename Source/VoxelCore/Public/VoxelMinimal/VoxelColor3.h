// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelHashUtilities.h"

struct FVoxelLinearColor3;

struct FVoxelColor3
{
	uint8 R;
	uint8 G;
	uint8 B;

	FVoxelColor3() = default;
	FORCEINLINE FVoxelColor3(const uint8 R, const uint8 G, const uint8 B)
		: R(R)
		, G(G)
		, B(B)
	{
	}
	FORCEINLINE explicit FVoxelColor3(EForceInit)
	{
		R = G = B = 0;
	}
	FORCEINLINE explicit FVoxelColor3(const FColor& Color)
		: R(Color.R)
		, G(Color.G)
		, B(Color.B)
	{
	}
	FORCEINLINE explicit operator FColor() const
	{
		return FColor(R, G, B);
	}

	FVoxelLinearColor3 ToLinear() const;
	FVoxelLinearColor3 ToLinear_Raw() const;

	friend FArchive& operator<<(FArchive& Ar, FVoxelColor3& Color)
	{
		Ar << Color.R;
		Ar << Color.G;
		Ar << Color.B;
		return Ar;
	}

	FORCEINLINE bool operator==(const FVoxelColor3& Other) const
	{
		return
			R == Other.R &&
			G == Other.G &&
			B == Other.B;
	}
	FORCEINLINE bool operator!=(const FVoxelColor3& Other) const
	{
		return
			R != Other.R ||
			G != Other.G ||
			B != Other.B;
	}

	FORCEINLINE uint32 AsInt() const
	{
		return
			(uint32(R) << 0) |
			(uint32(G) << 8) |
			(uint32(B) << 16);
	}
	FORCEINLINE friend uint32 GetTypeHash(const FVoxelColor3& Color)
	{
		return Color.AsInt();
	}
};
static_assert(sizeof(FVoxelColor3) == 3, "");

struct FVoxelLinearColor3
{
	float R;
	float G;
	float B;

	FVoxelLinearColor3() = default;
	FORCEINLINE FVoxelLinearColor3(const float R, const float G, const float B)
		: R(R)
		, G(G)
		, B(B)
	{
	}
	FORCEINLINE explicit FVoxelLinearColor3(EForceInit)
	{
		R = G = B = 0;
	}
	FORCEINLINE explicit FVoxelLinearColor3(const FLinearColor& Color)
		: R(Color.R)
		, G(Color.G)
		, B(Color.B)
	{
	}
	FORCEINLINE explicit operator FLinearColor() const
	{
		return FLinearColor(R, G, B, 1.f);
	}

	FVoxelColor3 ToSRGB() const;
	FVoxelColor3 ToSRGB_Raw() const;

	friend FArchive& operator<<(FArchive& Ar, FVoxelLinearColor3& Color)
	{
		Ar << Color.R;
		Ar << Color.G;
		Ar << Color.B;
		return Ar;
	}

	FORCEINLINE FVoxelLinearColor3 operator+(const FVoxelLinearColor3& Other) const
	{
		return FVoxelLinearColor3(
			R + Other.R,
			G + Other.G,
			B + Other.B);
	}
	FORCEINLINE FVoxelLinearColor3& operator+=(const FVoxelLinearColor3& Other)
	{
		R += Other.R;
		G += Other.G;
		B += Other.B;
		return *this;
	}

	FORCEINLINE FVoxelLinearColor3 operator-(const FVoxelLinearColor3& Other) const
	{
		return FVoxelLinearColor3(
			R - Other.R,
			G - Other.G,
			B - Other.B);
	}
	FORCEINLINE FVoxelLinearColor3& operator-=(const FVoxelLinearColor3& Other)
	{
		R -= Other.R;
		G -= Other.G;
		B -= Other.B;
		return *this;
	}

	FORCEINLINE FVoxelLinearColor3 operator*(const FVoxelLinearColor3& Other) const
	{
		return FVoxelLinearColor3(
			R * Other.R,
			G * Other.G,
			B * Other.B);
	}
	FORCEINLINE FVoxelLinearColor3& operator*=(const FVoxelLinearColor3& Other)
	{
		R *= Other.R;
		G *= Other.G;
		B *= Other.B;
		return *this;
	}

	FORCEINLINE FVoxelLinearColor3 operator*(const float Scalar) const
	{
		return FVoxelLinearColor3(
			R * Scalar,
			G * Scalar,
			B * Scalar);
	}

	FORCEINLINE FVoxelLinearColor3& operator*=(const float Scalar)
	{
		R *= Scalar;
		G *= Scalar;
		B *= Scalar;
		return *this;
	}

	FORCEINLINE FVoxelLinearColor3 operator/(const FVoxelLinearColor3& Other) const
	{
		return FVoxelLinearColor3(
			R / Other.R,
			G / Other.G,
			B / Other.B);
	}
	FORCEINLINE FVoxelLinearColor3& operator/=(const FVoxelLinearColor3& Other)
	{
		R /= Other.R;
		G /= Other.G;
		B /= Other.B;
		return *this;
	}

	FORCEINLINE FVoxelLinearColor3 operator/(const float Scalar) const
	{
		return FVoxelLinearColor3(
			R / Scalar,
			G / Scalar,
			B / Scalar);
	}
	FORCEINLINE FVoxelLinearColor3& operator/=(const float Scalar)
	{
		R /= Scalar;
		G /= Scalar;
		B /= Scalar;
		return *this;
	}

	FORCEINLINE bool operator==(const FVoxelLinearColor3& Other) const
	{
		return
			R == Other.R &&
			G == Other.G &&
			B == Other.B;
	}
	FORCEINLINE bool operator!=(const FVoxelLinearColor3& Other) const
	{
		return
			R != Other.R ||
			G != Other.G ||
			B != Other.B;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelLinearColor3& Color)
	{
		return FVoxelUtilities::MurmurHash(Color);
	}
};
static_assert(sizeof(FVoxelLinearColor3) == 3 * sizeof(float), "");

FORCEINLINE FVoxelLinearColor3 operator*(const float Scalar, const FVoxelLinearColor3& Color)
{
	return Color.operator*(Scalar);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FVoxelLinearColor3 FVoxelColor3::ToLinear() const
{
	return
	{
		FLinearColor::sRGBToLinearTable[R],
		FLinearColor::sRGBToLinearTable[G],
		FLinearColor::sRGBToLinearTable[B],
	};
}

FORCEINLINE FVoxelLinearColor3 FVoxelColor3::ToLinear_Raw() const
{
	return
	{
		FVoxelUtilities::UINT8ToFloat(R),
		FVoxelUtilities::UINT8ToFloat(G),
		FVoxelUtilities::UINT8ToFloat(B),
	};
}

FORCEINLINE FVoxelColor3 FVoxelLinearColor3::ToSRGB() const
{
	float FloatR = FMath::Clamp(R, 0.0f, 1.0f);
	float FloatG = FMath::Clamp(G, 0.0f, 1.0f);
	float FloatB = FMath::Clamp(B, 0.0f, 1.0f);

	FloatR = FloatR <= 0.0031308f ? FloatR * 12.92f : FMath::Pow(FloatR, 1.0f / 2.4f) * 1.055f - 0.055f;
	FloatG = FloatG <= 0.0031308f ? FloatG * 12.92f : FMath::Pow(FloatG, 1.0f / 2.4f) * 1.055f - 0.055f;
	FloatB = FloatB <= 0.0031308f ? FloatB * 12.92f : FMath::Pow(FloatB, 1.0f / 2.4f) * 1.055f - 0.055f;

	return
	{
		uint8(FMath::FloorToInt(FloatR * 255.999f)),
		uint8(FMath::FloorToInt(FloatG * 255.999f)),
		uint8(FMath::FloorToInt(FloatB * 255.999f))
	};
}

FORCEINLINE FVoxelColor3 FVoxelLinearColor3::ToSRGB_Raw() const
{
	return
	{
		FVoxelUtilities::FloatToUINT8(R),
		FVoxelUtilities::FloatToUINT8(G),
		FVoxelUtilities::FloatToUINT8(B)
	};
}