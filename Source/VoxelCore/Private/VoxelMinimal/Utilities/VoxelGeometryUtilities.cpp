// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FQuat FVoxelUtilities::MakeQuaternionFromEuler(const double Pitch, const double Yaw, const double Roll)
{
	double SinPitch;
	double CosPitch;
	FMath::SinCos(&SinPitch, &CosPitch, FMath::Fmod(Pitch, 360.0f) * PI / 360.f);

	double SinYaw;
	double CosYaw;
	FMath::SinCos(&SinYaw, &CosYaw, FMath::Fmod(Yaw, 360.0f) * PI / 360.f);

	double SinRoll;
	double CosRoll;
	FMath::SinCos(&SinRoll, &CosRoll, FMath::Fmod(Roll, 360.0f) * PI / 360.f);

	const FQuat Quat = FQuat(
		CosRoll * SinPitch * SinYaw - SinRoll * CosPitch * CosYaw,
		-CosRoll * SinPitch * CosYaw - SinRoll * CosPitch * SinYaw,
		CosRoll * CosPitch * SinYaw - SinRoll * SinPitch * CosYaw,
		CosRoll * CosPitch * CosYaw + SinRoll * SinPitch * SinYaw);

	checkVoxelSlow(Quat.Equals(FRotator(Pitch, Yaw, Roll).Quaternion()));
	return Quat;
}

FQuat FVoxelUtilities::MakeQuaternionFromBasis(const FVector& X, const FVector& Y, const FVector& Z)
{
	FQuat Quat;

	if (X.X + Y.Y + Z.Z > 0.0f)
	{
		const double InvS = FMath::InvSqrt(X.X + Y.Y + Z.Z + 1.0f);
		const double S = 0.5f * InvS;

		Quat.X = (Y.Z - Z.Y) * S;
		Quat.Y = (Z.X - X.Z) * S;
		Quat.Z = (X.Y - Y.X) * S;
		Quat.W = 0.5f * (1.f / InvS);
	}
	else if (X.X > Y.Y && X.X > Z.Z)
	{
		const double InvS = FMath::InvSqrt(X.X - Y.Y - Z.Z + 1.0f);
		const double S = 0.5f * InvS;

		Quat.X = 0.5f * (1.f / InvS);
		Quat.Y = (X.Y + Y.X) * S;
		Quat.Z = (X.Z + Z.X) * S;
		Quat.W = (Y.Z - Z.Y) * S;
	}
	else if (Y.Y > X.X && Y.Y > Z.Z)
	{
		const double InvS = FMath::InvSqrt(Y.Y - Z.Z - X.X + 1.0f);
		const double S = 0.5f * InvS;

		Quat.Y = 0.5f * (1.f / InvS);
		Quat.Z = (Y.Z + Z.Y) * S;
		Quat.X = (Y.X + X.Y) * S;
		Quat.W = (Z.X - X.Z) * S;
	}
	else
	{
		checkVoxelSlow(Z.Z >= X.X && Z.Z >= Y.Y);

		const double InvS = FMath::InvSqrt(Z.Z - X.X - Y.Y + 1.0f);
		const double S = 0.5f * InvS;

		Quat.Z = 0.5f * (1.f / InvS);
		Quat.X = (Z.X + X.Z) * S;
		Quat.Y = (Z.Y + Y.Z) * S;
		Quat.W = (X.Y - Y.X) * S;
	}

	checkVoxelSlow(Quat.Equals(FMatrix(X, Y, Z, FVector::Zero()).ToQuat()));
	return Quat;
}

FQuat FVoxelUtilities::MakeQuaternionFromZ(const FVector& Z)
{
	const FVector NewZ = Z.GetSafeNormal();

	// Try to use up if possible
	const FVector UpVector = FMath::Abs(NewZ.Z) < (1.f - KINDA_SMALL_NUMBER) ? FVector(0, 0, 1.f) : FVector(1.f, 0, 0);

	const FVector NewX = FVector::CrossProduct(UpVector, NewZ).GetSafeNormal();
	const FVector NewY = FVector::CrossProduct(NewZ, NewX);

	const FQuat Quat = MakeQuaternionFromBasis(NewX, NewY, NewZ);
	checkVoxelSlow(Quat.Equals(FRotationMatrix::MakeFromZ(Z).ToQuat()));
	return Quat;
}