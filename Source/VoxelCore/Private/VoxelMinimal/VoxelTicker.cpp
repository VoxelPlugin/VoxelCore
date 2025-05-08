// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Containers/Ticker.h"

struct FVoxelTickerData
{
	FVoxelTicker* Ticker = nullptr;
	bool bIsDestroyed = false;
};

class FVoxelTickerManager : public FTSTickerObjectBase
{
public:
	TVoxelArray<TUniquePtr<FVoxelTickerData>> TickerDatas;

	FVoxelTickerManager() = default;

	void Tick()
	{
		VOXEL_SCOPE_COUNTER_FORMAT("FVoxelTicker::Tick Num=%d", TickerDatas.Num());
		check(IsInGameThread());

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
	}

	//~ Begin FTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override
	{
		Tick();
		return true;
	}
	//~ End FTickerObjectBase Interface
};
FVoxelTickerManager* GVoxelTickerManager = nullptr;

VOXEL_RUN_ON_STARTUP(Game, 999)
{
	GVoxelTickerManager = new FVoxelTickerManager();

	Voxel::OnForceTick.AddLambda([]
	{
		if (GVoxelTickerManager)
		{
			GVoxelTickerManager->Tick();
		}
	});
}

void DestroyVoxelTickers()
{
	delete GVoxelTickerManager;
	GVoxelTickerManager = nullptr;
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

	TUniquePtr<FVoxelTickerData> UniqueTickerData = MakeUnique<FVoxelTickerData>();
	UniqueTickerData->Ticker = this;
	TickerData = UniqueTickerData.Get();

	check(GVoxelTickerManager);
	GVoxelTickerManager->TickerDatas.Add(MoveTemp(UniqueTickerData));
}

FVoxelTicker::~FVoxelTicker()
{
	ensure(IsInGameThread());

	if (!ensure(TickerData) ||
		!ensure(GVoxelTickerManager))
	{
		return;
	}

	checkVoxelSlow(TickerData->Ticker == this);
	TickerData->Ticker = nullptr;

	TickerData->bIsDestroyed = true;
}