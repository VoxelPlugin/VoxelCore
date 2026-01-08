// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkData.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

void FVoxelBulkData::SerializeAsBytes(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	FObjectAndNameAsStringProxyArchive Proxy(Ar, true);
	Proxy.SetIsSaving(Ar.IsSaving());
	Proxy.SetIsLoading(Ar.IsLoading());
	Serialize(Proxy);
}