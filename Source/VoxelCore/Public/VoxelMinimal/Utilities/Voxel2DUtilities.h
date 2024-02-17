// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Math/TransformCalculus2D.h"

template<typename T>
FORCEINLINE TTransform2<T> operator*(const TTransform2<T>& A, const TTransform2<T>& B)
{
	return A.Concatenate(B);
}

template<typename T>
FORCEINLINE TTransform2<T>& operator*=(TTransform2<T>& Transform, const TTransform2<T>& Other)
{
	Transform = Transform.Concatenate(Other);
	return Transform;
}

template<typename T>
FORCEINLINE TTransform2<T> MakeTransform2(
	const TQuat2<T>& Rotation,
	const UE::Math::TVector2<T>& Translation = UE::Math::TVector2<T>(0.f),
	const UE::Math::TVector2<T>& Scale = UE::Math::TVector2<T>(1.f))
{
	// Constructor taking a FQuat2d is broken
	TTransform2<T> RotationTransform;
	{
		struct FMatrix2
		{
			T M[2][2];
		};
		FMatrix2& Matrix = ReinterpretCastRef<FMatrix2>(ConstCast(RotationTransform.GetMatrix()));

		const T CosAngle = Rotation.GetVector().X;
		const T SinAngle = Rotation.GetVector().Y;

		Matrix.M[0][0] = CosAngle;
		Matrix.M[0][1] = SinAngle;
		Matrix.M[1][0] = -SinAngle;
		Matrix.M[1][1] = CosAngle;
	}

	TTransform2<T> Transform = TTransform2<T>(TScale2<T>(Scale));
	Transform *= RotationTransform;
	Transform *= TTransform2<T>(Translation);
	return Transform;
}