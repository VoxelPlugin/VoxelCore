// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelObjectWithGuid.generated.h"

UCLASS(Abstract)
class VOXELCORE_API UVoxelObjectWithGuid : public UObject
{
	GENERATED_BODY()

public:
	FGuid GetGuid() const;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	//~ End UObject Interface

private:
	UPROPERTY()
	FGuid PrivateGuid;

	void UpdateGuid();
};