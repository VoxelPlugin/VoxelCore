// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelLatentAction.h"
#include "VoxelTaskContext.h"
#include "Kismet/BlueprintFunctionLibrary.h"

FVoxelLatentAction::FVoxelLatentAction(const FLatentActionInfo& LatentInfo)
	: ExecutionFunction(LatentInfo.ExecutionFunction)
	, OutputLink(LatentInfo.Linkage)
	, CallbackTarget(LatentInfo.CallbackTarget)
	, TaskContext(FVoxelTaskContext::Create(STATIC_FNAME("FVoxelLatentAction")))
{
}

FVoxelLatentAction::~FVoxelLatentAction()
{
	TaskContext->CancelTasks();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelLatentAction::UpdateOperation(FLatentResponse& Response)
{
	VOXEL_FUNCTION_COUNTER();

	if (Future.IsComplete())
	{
		ensureVoxelSlow(TaskContext->IsComplete());

		Response.FinishAndTriggerIf(
			true,
			ExecutionFunction,
			OutputLink,
			CallbackTarget);
	}
}

void FVoxelLatentAction::NotifyObjectDestroyed()
{
	TaskContext->CancelTasks();
}

void FVoxelLatentAction::NotifyActionAborted()
{
	TaskContext->CancelTasks();
}

#if WITH_EDITOR
FString FVoxelLatentAction::GetDescription() const
{
	return "FVoxelLatentAction";
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelLatentAction::Execute(
	const UObject* WorldContextObject,
	const FLatentActionInfo& LatentInfo,
	const bool bExecuteIfAlreadyPending,
	const TFunctionRef<FVoxelFuture()> Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		VOXEL_MESSAGE(Error, "World is null, cannot execute async");
		return;
	}
	FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

	if (!bExecuteIfAlreadyPending &&
		LatentActionManager.FindExistingAction<FVoxelLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID))
	{
		return;
	}

	FVoxelLatentAction* Action = new FVoxelLatentAction(LatentInfo);

	{
		FVoxelTaskScope Scope(*Action->TaskContext);
		Action->Future = Lambda();
	}

	LatentActionManager.AddNewAction(
		LatentInfo.CallbackTarget,
		LatentInfo.UUID,
		Action);
}