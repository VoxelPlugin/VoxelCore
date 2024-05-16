// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Containers/Ticker.h"

struct FVoxelTickerData
{
	FVoxelTicker* Ticker = nullptr;
	bool bIsDestroyed = false;
};

class VOXELCORE_API FVoxelTickerManager : public FTSTickerObjectBase
{
public:
	TVoxelArray<TVoxelUniquePtr<FVoxelTickerData>> TickerDatas;

	FVoxelTickerManager() = default;

	//~ Begin FTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override
	{
		VOXEL_SCOPE_COUNTER_FORMAT("FVoxelTicker::Tick Num=%d", TickerDatas.Num());

		for (int32 Index = 0; Index < TickerDatas.Num(); Index++)
		{
			const FVoxelTickerData& TickerData = *TickerDatas[Index];
			if (TickerData.bIsDestroyed)
			{
				TickerDatas.RemoveAtSwap(Index);
				Index--;
				continue;
			}

			TickerData.Ticker->Tick();
		}

		return true;
	}
	//~ End FTickerObjectBase Interface
};

FVoxelTickerManager* GVoxelTickerManager = nullptr;

VOXEL_RUN_ON_STARTUP(Game, 999)
{
	GVoxelTickerManager = new FVoxelTickerManager();

	GOnVoxelModuleUnloaded_DoCleanup.AddLambda([]
	{
		if (GVoxelTickerManager)
		{
			delete GVoxelTickerManager;
			GVoxelTickerManager = nullptr;
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTicker::FVoxelTicker()
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(IsInGameThread()))
	{
		return;
	}

	TVoxelUniquePtr<FVoxelTickerData> UniqueTickerData = MakeVoxelUnique<FVoxelTickerData>();
	UniqueTickerData->Ticker = this;
	TickerData = UniqueTickerData.Get();

	check(GVoxelTickerManager);
	GVoxelTickerManager->TickerDatas.Add(MoveTemp(UniqueTickerData));
}

FVoxelTicker::~FVoxelTicker()
{
	ensure(IsInGameThread());

	if (!ensure(TickerData))
	{
		return;
	}

	TickerData->bIsDestroyed = true;
}