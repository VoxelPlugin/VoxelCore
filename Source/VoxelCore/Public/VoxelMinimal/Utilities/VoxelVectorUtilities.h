// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

namespace FVoxelUtilities
{
	template<typename T>
	FORCEINLINE int64 FloorToInt64(T Value)
	{
		return FMath::FloorToInt64(Value);
	}
	template<typename T>
	FORCEINLINE int64 FloorToInt32(T Value)
	{
#if VOXEL_DEBUG
		const int64 Int = FMath::FloorToInt64(Value);
		ensureVoxelSlow(MIN_int32 <= Int && Int <= MAX_int32);
#endif
		return FMath::FloorToInt32(Value);
	}

	template<typename T>
	FORCEINLINE int64 CeilToInt64(T Value)
	{
		return FMath::CeilToInt64(Value);
	}
	template<typename T>
	FORCEINLINE int64 CeilToInt32(T Value)
	{
#if VOXEL_DEBUG
		const int64 Int = FMath::CeilToInt64(Value);
		ensureVoxelSlow(MIN_int32 <= Int && Int <= MAX_int32);
#endif
		return FMath::CeilToInt32(Value);
	}

	template<typename T>
	FORCEINLINE int64 RoundToInt64(T Value)
	{
		return FMath::RoundToInt64(Value);
	}
	template<typename T>
	FORCEINLINE int64 RoundToInt32(T Value)
	{
#if VOXEL_DEBUG
		const int64 Int = FMath::RoundToInt64(Value);
		ensureVoxelSlow(MIN_int32 <= Int && Int <= MAX_int32);
#endif
		return FMath::RoundToInt32(Value);
	}

	template<typename Vector, typename Result = Vector>
	using TEnableIfAnyVector2 = typename TEnableIf<std::is_same_v<Vector, FIntPoint> || std::is_same_v<Vector, FVector2f> || std::is_same_v<Vector, FVector2d>, Result>::Type;
	template<typename Vector, typename Result = Vector>
	using TEnableIfAnyVector3 = typename TEnableIf<std::is_same_v<Vector, FIntVector> || std::is_same_v<Vector, FVector3f> || std::is_same_v<Vector, FVector3d>, Result>::Type;

	template<typename Vector, typename Result = Vector>
	using TEnableIfFloatVector2 = typename TEnableIf<std::is_same_v<Vector, FVector2f> || std::is_same_v<Vector, FVector2d>, Result>::Type;
	template<typename Vector, typename Result = Vector>
	using TEnableIfFloatVector3 = typename TEnableIf<std::is_same_v<Vector, FVector3f> || std::is_same_v<Vector, FVector3d>, Result>::Type;

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType> RoundToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::RoundToFloat(Vector.X),
			FMath::RoundToFloat(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType> RoundToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::RoundToFloat(Vector.X),
			FMath::RoundToFloat(Vector.Y),
			FMath::RoundToFloat(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType> FloorToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::FloorToFloat(Vector.X),
			FMath::FloorToFloat(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType> FloorToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::FloorToFloat(Vector.X),
			FMath::FloorToFloat(Vector.Y),
			FMath::FloorToFloat(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType> CeilToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::CeilToFloat(Vector.X),
			FMath::CeilToFloat(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType> CeilToFloat(const VectorType& Vector)
	{
		return VectorType(
			FMath::CeilToFloat(Vector.X),
			FMath::CeilToFloat(Vector.Y),
			FMath::CeilToFloat(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType, FIntPoint> RoundToInt(const VectorType& Vector)
	{
		return FIntPoint(
			RoundToInt32(Vector.X),
			RoundToInt32(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType, FIntVector> RoundToInt(const VectorType& Vector)
	{
		return FIntVector(
			RoundToInt32(Vector.X),
			RoundToInt32(Vector.Y),
			RoundToInt32(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType, FIntPoint> FloorToInt(const VectorType& Vector)
	{
		return FIntPoint(
			FloorToInt32(Vector.X),
			FloorToInt32(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType, FIntVector> FloorToInt(const VectorType& Vector)
	{
		return FIntVector(
			FloorToInt32(Vector.X),
			FloorToInt32(Vector.Y),
			FloorToInt32(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector2<VectorType, FIntPoint> CeilToInt(const VectorType& Vector)
	{
		return FIntPoint(
			CeilToInt32(Vector.X),
			CeilToInt32(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfFloatVector3<VectorType, FIntVector> CeilToInt(const VectorType& Vector)
	{
		return FIntVector(
			CeilToInt32(Vector.X),
			CeilToInt32(Vector.Y),
			CeilToInt32(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType> Abs(const VectorType& Vector)
	{
		return VectorType(
			FMath::Abs(Vector.X),
			FMath::Abs(Vector.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType> Abs(const VectorType& Vector)
	{
		return VectorType(
			FMath::Abs(Vector.X),
			FMath::Abs(Vector.Y),
			FMath::Abs(Vector.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType> ComponentMax(const VectorType& A, const VectorType& B)
	{
		return VectorType(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType> ComponentMax(const VectorType& A, const VectorType& B)
	{
		return VectorType(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType> ComponentMin(const VectorType& A, const VectorType& B)
	{
		return VectorType(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType> ComponentMin(const VectorType& A, const VectorType& B)
	{
		return VectorType(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template<typename VectorType>
	FORCEINLINE VectorType ComponentMin3(const VectorType& A, const VectorType& B, const VectorType& C)
	{
		return ComponentMin(A, ComponentMin(B, C));
	}
	template<typename VectorType>
	FORCEINLINE VectorType ComponentMax3(const VectorType& A, const VectorType& B, const VectorType& C)
	{
		return ComponentMax(A, ComponentMax(B, C));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType> Clamp(const VectorType& V, const VectorType& Min, const VectorType& Max)
	{
		return VectorType(
			FMath::Clamp(V.X, Min.X, Max.X),
			FMath::Clamp(V.Y, Min.Y, Max.Y));
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType> Clamp(const VectorType& V, const VectorType& Min, const VectorType& Max)
	{
		return VectorType(
			FMath::Clamp(V.X, Min.X, Max.X),
			FMath::Clamp(V.Y, Min.Y, Max.Y),
			FMath::Clamp(V.Z, Min.Z, Max.Z));
	}

	template<typename VectorType, typename ScalarTypeA, typename ScalarTypeB>
	FORCEINLINE TEnableIfAnyVector2<VectorType> Clamp(const VectorType& V, const ScalarTypeA& Min, const ScalarTypeB& Max)
	{
		return FVoxelUtilities::Clamp(V, VectorType(Min), VectorType(Max));
	}
	template<typename TVector, typename ScalarTypeA, typename ScalarTypeB>
	FORCEINLINE TEnableIfAnyVector3<TVector> Clamp(const TVector& V, const ScalarTypeA& Min, const ScalarTypeB& Max)
	{
		return FVoxelUtilities::Clamp(V, TVector(Min), TVector(Max));
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType, int32> GetSmallestAxis(const VectorType& Vector)
	{
		return Vector.X <= Vector.Y ? 0 : 1;
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector2<VectorType, int32> GetLargestAxis(const VectorType& Vector)
	{
		return Vector.X >= Vector.Y ? 0 : 1;
	}

	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType, int32> GetSmallestAxis(const VectorType& Vector)
	{
		if (Vector.X <= Vector.Y &&
			Vector.X <= Vector.Z)
		{
			return 0;
		}
		else if (Vector.Y <= Vector.Z)
		{
			return 1;
		}
		else
		{
			ensureVoxelSlow(Vector.Z <= Vector.X && Vector.Z <= Vector.Y);
			return 2;
		}
	}
	template<typename VectorType>
	FORCEINLINE TEnableIfAnyVector3<VectorType, int32> GetLargestAxis(const VectorType& Vector)
	{
		if (Vector.X >= Vector.Y &&
			Vector.X >= Vector.Z)
		{
			return 0;
		}
		else if (Vector.Y >= Vector.Z)
		{
			return 1;
		}
		else
		{
			ensureVoxelSlow(Vector.Z >= Vector.X && Vector.Z >= Vector.Y);
			return 2;
		}
	}
}