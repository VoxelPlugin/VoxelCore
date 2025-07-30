// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTickerWorldSubsystem.h"
#include "MoviePlayer.h"

TStatId UVoxelTickerWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVoxelTickerWorldSubsystem, STATGROUP_Tickables);
}

void UVoxelTickerWorldSubsystem::Tick(const float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();

	Super::Tick(DeltaTime);

	if (GEngine &&
		GEngine->IsInitialized() &&
		GetMoviePlayer() &&
		GetMoviePlayer()->IsStartupMoviePlaying())
	{
		// Force tick if we have a loading screen playing, as regular tickers aren't ticked then
		Voxel::ForceTick();
	}
}