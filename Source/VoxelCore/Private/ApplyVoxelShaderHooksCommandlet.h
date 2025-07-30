// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ApplyVoxelShaderHooksCommandlet.generated.h"

UCLASS()
class UApplyVoxelShaderHooksCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};