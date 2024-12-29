// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPromiseState.h"
#include "VoxelTaskDispatcherInterface.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

FORCEINLINE void FVoxelPromiseState::FContinuation::Execute(
	IVoxelTaskDispatcher& Dispatcher,
	const FVoxelPromiseState& NewValue)
{
	switch (Type)
	{
	default: VOXEL_ASSUME(false);
	case EType::Future:
	{
		FVoxelPromiseState& Future = *GetFuture();
		if (Future.bHasValue)
		{
			Future.Set(NewValue.GetSharedValueChecked());
		}
		else
		{
			Future.Set();
		}
	}
	break;
	case EType::VoidLambda:
	{
		Dispatcher.Dispatch(Thread, MoveTemp(GetVoidLambda()));
	}
	break;
	case EType::ValueLambda:
	{
		Dispatcher.Dispatch(Thread, [Lambda = MoveTemp(GetValueLambda()), Value = NewValue.GetSharedValueChecked()]
		{
			Lambda(Value);
		});
	}
	break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPromiseState::FVoxelPromiseState(
	IVoxelTaskDispatcher* DispatcherOverride,
	const bool bHasValue)
	: IVoxelPromiseState(bHasValue)
{
	IVoxelTaskDispatcher& Dispatcher = DispatcherOverride ? *DispatcherOverride : FVoxelTaskDispatcherScope::Get();

	ConstCast(DispatcherRef) = Dispatcher;

	if (Dispatcher.NumPromisesPtr)
	{
		Dispatcher.NumPromisesPtr->Increment();
	}

	if (Dispatcher.bTrackPromisesCallstacks)
	{
		VOXEL_SCOPE_LOCK(Dispatcher.CriticalSection);
		StackIndex = Dispatcher.StackFrames_RequiresLock.Add(FVoxelUtilities::GetStackFrames(4));
	}
}

FVoxelPromiseState::~FVoxelPromiseState()
{
#if VOXEL_DEBUG
	if (IsComplete())
	{
		checkVoxelSlow(Value.IsValid() == bHasValue);
		checkVoxelSlow(KeepAliveIndex == -1);
		checkVoxelSlow(StackIndex == -1);
		checkVoxelSlow(!Continuation_RequiresLock);
		return;
	}
	checkVoxelSlow(!Value.IsValid());

	const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = DispatcherRef.Pin();
	if (!Dispatcher)
	{
		return;
	}
	ensure(Dispatcher->IsExiting());
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromiseState::Set()
{
	checkVoxelSlow(!bHasValue);

	const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = DispatcherRef.Pin();
	if (!ensureVoxelSlow(Dispatcher))
	{
		return;
	}

	SetImpl(*Dispatcher);
}

void FVoxelPromiseState::Set(const FSharedVoidRef& NewValue)
{
	checkVoxelSlow(bHasValue);

	const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = DispatcherRef.Pin();
	if (!Dispatcher)
	{
		// Will be null when called as a continuation from a different dispatcher
		return;
	}

	checkVoxelSlow(!Value);
	Value = NewValue;

	SetImpl(*Dispatcher);
}

void FVoxelPromiseState::AddContinuation(TUniquePtr<FContinuation> Continuation)
{
	const TSharedPtr<IVoxelTaskDispatcher> DispatcherPtr = DispatcherRef.Pin();
	if (!ensureVoxelSlow(DispatcherPtr))
	{
		return;
	}
	IVoxelTaskDispatcher& Dispatcher = *DispatcherPtr;

	ON_SCOPE_EXIT
	{
		if (Continuation)
		{
			checkVoxelSlow(IsComplete());
			Continuation->Execute(Dispatcher, *this);
		}
	};

	if (IsComplete())
	{
		return;
	}

	VOXEL_SCOPE_LOCK_ATOMIC(bIsLocked);

	if (IsComplete())
	{
		return;
	}

	// Ensure we're kept alive until all continuations are fired
	if (KeepAliveIndex == -1)
	{
		VOXEL_SCOPE_LOCK(Dispatcher.CriticalSection);
		KeepAliveIndex = Dispatcher.PromisesToKeepAlive_RequiresLock.Add(AsShared());
	}

	checkVoxelSlow(!Continuation->NextContinuation);
	Continuation->NextContinuation = MoveTemp(Continuation_RequiresLock);

	checkVoxelSlow(!Continuation_RequiresLock);
	Continuation_RequiresLock = MoveTemp(Continuation);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromiseState::SetImpl(IVoxelTaskDispatcher& Dispatcher)
{
	checkVoxelSlow(!bIsComplete.Get());
	bIsComplete.Set(true);

	ON_SCOPE_EXIT
	{
		if (Dispatcher.NumPromisesPtr)
		{
			Dispatcher.NumPromisesPtr->Decrement();
		}

		if (KeepAliveIndex != -1)
		{
			VOXEL_SCOPE_LOCK(Dispatcher.CriticalSection);
			Dispatcher.PromisesToKeepAlive_RequiresLock.RemoveAt(KeepAliveIndex);

			KeepAliveIndex = -1;
		}

		if (StackIndex != -1)
		{
			VOXEL_SCOPE_LOCK(Dispatcher.CriticalSection);
			Dispatcher.StackFrames_RequiresLock.RemoveAt(StackIndex);

			StackIndex = -1;
		}
	};

	VOXEL_SCOPE_LOCK_ATOMIC(bIsLocked);

	TUniquePtr<FContinuation> Continuation = MoveTemp(Continuation_RequiresLock);
	while (Continuation)
	{
		Continuation->Execute(Dispatcher, *this);
		Continuation = MoveTemp(Continuation->NextContinuation);
	}
}