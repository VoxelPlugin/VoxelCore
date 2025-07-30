// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

FTransform FVoxelUtilities::MakeTransformSafe(const FMatrix& Matrix)
{
	const FTransform Transform(Matrix);
	ensure(Transform.ToMatrixWithScale().Equals(Matrix));
	return Transform;
}

FTransform2d FVoxelUtilities::MakeTransform2(
	const FQuat2d& Rotation,
	const FVector2D& Translation,
	const FVector2D& Scale)
{
	// Constructor taking a FQuat2d is broken
	FTransform2d RotationTransform;
	{
		struct FMatrix2
		{
			double M[2][2];
		};
		FMatrix2& Matrix = ReinterpretCastRef<FMatrix2>(ConstCast(RotationTransform.GetMatrix()));

		const double CosAngle = Rotation.GetVector().X;
		const double SinAngle = Rotation.GetVector().Y;

		Matrix.M[0][0] = CosAngle;
		Matrix.M[0][1] = SinAngle;
		Matrix.M[1][0] = -SinAngle;
		Matrix.M[1][1] = CosAngle;
	}

	FTransform2d Transform = FTransform2d(FScale2d(Scale));
	Transform *= RotationTransform;
	Transform *= FTransform2d(Translation);
	return Transform;
}

FTransform2d FVoxelUtilities::MakeTransform2(const FTransform& Transform)
{
	FQuat Swing;
	FQuat Twist;
	Transform.GetRotation().ToSwingTwist(
		FVector::UnitZ(),
		Swing,
		Twist);

	// Go through euler angles to avoid flipping at 240 degrees
	const double AngleZ = FMath::DegreesToRadians(Twist.Euler().Z);

	return MakeTransform2(
		FQuat2d(AngleZ),
		FVector2D(Transform.GetTranslation()),
		FVector2D(Transform.GetScale3D()));
}

FTransform2f FVoxelUtilities::MakeTransform2f(const FTransform2d& Transform)
{
	float A;
	float B;
	float C;
	float D;
	Transform.GetMatrix().GetMatrix(A, B, C, D);

	return FTransform2f(
		FMatrix2x2f(A, B, C, D),
		FVector2f(Transform.GetTranslation()));
}