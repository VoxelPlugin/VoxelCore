// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "LatentActions.h"

#if !CPP
USTRUCT(NoExport, BlueprintType)
struct FVoxelFuture
{
	GENERATED_BODY()
};
#endif

class VOXELCORE_API FVoxelLatentAction : public FPendingLatentAction
{
public:
	const FName ExecutionFunction;
	const int32 OutputLink;
	const FWeakObjectPtr CallbackTarget;
	const TSharedRef<FVoxelTaskContext> TaskContext;

	explicit FVoxelLatentAction(const FLatentActionInfo& LatentInfo);
	virtual ~FVoxelLatentAction() override;

public:
	//~ Begin FPendingLatentAction Interface
	virtual void UpdateOperation(FLatentResponse& Response) override;
	virtual void NotifyObjectDestroyed() override;
	virtual void NotifyActionAborted() override;
#if WITH_EDITOR
	virtual FString GetDescription() const override;
#endif
	//~ End FPendingLatentAction Interface

private:
	FVoxelFuture Future;

public:
	static void Execute(
		const UObject* WorldContextObject,
		const FLatentActionInfo& LatentInfo,
		bool bExecuteIfAlreadyPending,
		TFunctionRef<FVoxelFuture()> Lambda);
};