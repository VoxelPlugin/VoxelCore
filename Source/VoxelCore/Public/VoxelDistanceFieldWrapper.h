// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "DistanceFieldAtlas.h"

class VOXELCORE_API FVoxelDistanceFieldWrapper
{
public:
	using FBrick = TVoxelStaticArray<uint8, DistanceField::BrickSize * DistanceField::BrickSize * DistanceField::BrickSize>;

	class VOXELCORE_API FMip
	{
	public:
		void Initialize(const FVoxelDistanceFieldWrapper& Wrapper);

		bool IsValidPosition(const FIntVector& Position) const;
		TSharedPtr<FBrick> FindBrick(const FIntVector& Position);
		TSharedRef<FBrick> FindOrAddBrick(const FIntVector& Position);

		void AddBrick(
			const FIntVector& Position,
			const TSharedRef<FBrick>& Brick);
		TVoxelArrayView<TSharedPtr<FBrick>> GetBricks()
		{
			return Bricks;
		}

		FORCEINLINE uint8 QuantizeDistance(const float Distance) const
		{
			// Transform to the tracing shader Volume space
			const float VolumeSpaceDistance = Distance * LocalToVolumeScale;
			// Transform to the Distance Field texture's space
			const float RescaledDistance = (VolumeSpaceDistance - DistanceFieldToVolumeScaleBias.Y) / DistanceFieldToVolumeScaleBias.X;
			checkVoxelSlow(DistanceField::DistanceFieldFormat == PF_G8);

			return FMath::Clamp<int32>(FMath::FloorToInt(RescaledDistance * 255.0f + .5f), 0, 255);
		}

		float GetLocalToVolumeScale() const
		{
			return LocalToVolumeScale;
		}

		const FVector2D& GetDistanceFieldToVolumeScaleBias() const
		{
			return DistanceFieldToVolumeScaleBias;
		}

		const FIntVector& GetIndirectionSize() const
		{
			return IndirectionSize;
		}

	private:
		float LocalToVolumeScale = 0.f;
		FVector2D DistanceFieldToVolumeScaleBias = FVector2D::ZeroVector;
		FIntVector IndirectionSize = FIntVector::ZeroValue;
		TVoxelArray<TSharedPtr<FBrick>> Bricks;

		friend class FVoxelDistanceFieldWrapper;
	};

	const FBox LocalSpaceMeshBounds;

	TVoxelStaticArray<FMip, DistanceField::NumMips> Mips;

	explicit FVoxelDistanceFieldWrapper(
		const FBox& LocalSpaceMeshBounds)
		: LocalSpaceMeshBounds(LocalSpaceMeshBounds)
	{
	}

	void SetSize(const FIntVector& Mip0IndirectionSize);
	TSharedRef<FDistanceFieldVolumeData> Build() const;
};