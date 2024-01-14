// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelObjectWithGuid.generated.h"

UCLASS(Abstract)
class VOXELCORE_API UVoxelObjectWithGuid : public UObject
{
	GENERATED_BODY()

public:
	FORCEINLINE FGuid GetGuid() const
	{
		return PrivateGuid;
	}

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	//~ End UObject Interface

private:
	UPROPERTY()
	FGuid PrivateGuid;
};