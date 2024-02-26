// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Math/TransformCalculus2D.h"

namespace FVoxelUtilities
{
	// Will ensure if matrix cannot be represented by a transform
	VOXELCORE_API FTransform MakeTransformSafe(const FMatrix& Matrix);

	VOXELCORE_API FTransform2d MakeTransform2(
		const FQuat2d& Rotation,
		const FVector2D& Translation = FVector2D(0.f),
		const FVector2D& Scale = FVector2D(1.f));

	VOXELCORE_API FTransform2d MakeTransform2(const FTransform& Transform);
	VOXELCORE_API FTransform2d MakeTransform2(const FMatrix& Transform);
}

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