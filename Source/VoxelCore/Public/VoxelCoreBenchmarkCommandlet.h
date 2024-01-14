// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VoxelCoreBenchmarkCommandlet.generated.h"

UCLASS()
class UVoxelCoreBenchmarkCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};