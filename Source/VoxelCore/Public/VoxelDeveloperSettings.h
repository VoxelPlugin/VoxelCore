// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxelDeveloperSettings.generated.h"

UCLASS(Abstract)
class VOXELCORE_API UVoxelDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UVoxelDeveloperSettings();

    //~ Begin UDeveloperSettings Interface
    virtual FName GetContainerName() const override;
    virtual void PostInitProperties() override;
    virtual void PostCDOContruct() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    //~ End UDeveloperSettings Interface
};