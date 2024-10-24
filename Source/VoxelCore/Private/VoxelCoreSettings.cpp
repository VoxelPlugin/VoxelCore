// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelCoreSettings.h"

#if WITH_EDITOR
void UVoxelCoreSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_OWN_MEMBER_NAME(FramesToAverage))
	{
		FramesToAverage = FMath::RoundUpToPowerOfTwo(FramesToAverage);
	}
}
#endif