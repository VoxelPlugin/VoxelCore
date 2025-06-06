// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelAxis.generated.h"

UENUM(BlueprintType)
enum class EVoxelAxis : uint8
{
	X UMETA(Icon = "StaticMeshEditor.ToggleShowTangents"),
	Y UMETA(Icon = "StaticMeshEditor.SetShowBinormals"),
	Z UMETA(Icon = "StaticMeshEditor.SetShowNormals")
};