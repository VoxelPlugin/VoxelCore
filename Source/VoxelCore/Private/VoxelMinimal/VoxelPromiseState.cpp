// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPromiseState.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

FORCEINLINE void FVoxelPromiseState::FContinuation::Execute(
	FVoxelTaskContext& Context,
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
		Context.Dispatch(Thread, MoveTemp(GetVoidLambda()));
	}
	break;
	case EType::ValueLambda:
	{
		Context.Dispatch(Thread, [Lambda = MoveTemp(GetValueLambda()), Value = NewValue.GetSharedValueChecked()]
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
	FVoxelTaskContext* ContextOverride,
	const bool bHasValue)
	: IVoxelPromiseState(bHasValue)
{
	FVoxelTaskContext& Context = ContextOverride ? *ContextOverride : FVoxelTaskScope::GetContext();

	ConstCast(ContextWeakRef) = Context;

	Context.NumPromises.Increment();

	if (Context.bTrackPromisesCallstacks)
	{
		VOXEL_SCOPE_LOCK(Context.CriticalSection);
		StackIndex = Context.StackFrames_RequiresLock.Add(FVoxelUtilities::GetStackFrames(4));
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

	const TUniquePtr<FVoxelTaskContextStrongRef> ContextStrongRef = ContextWeakRef.Pin();
	if (!ContextStrongRef)
	{
		return;
	}
	ensure(ContextStrongRef->Context.IsExiting());
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromiseState::Set()
{
	checkVoxelSlow(!bHasValue);

	const TUniquePtr<FVoxelTaskContextStrongRef> ContextStrongRef = ContextWeakRef.Pin();
	if (!ContextStrongRef)
	{
		return;
	}

	SetImpl(ContextStrongRef->Context);
}

void FVoxelPromiseState::Set(const FSharedVoidRef& NewValue)
{
	checkVoxelSlow(bHasValue);

	const TUniquePtr<FVoxelTaskContextStrongRef> ContextStrongRef = ContextWeakRef.Pin();
	if (!ContextStrongRef)
	{
		// Will be null when called as a continuation from a different context
		return;
	}

	checkVoxelSlow(!Value);
	Value = NewValue;

	SetImpl(ContextStrongRef->Context);
}

void FVoxelPromiseState::AddContinuation(TUniquePtr<FContinuation> Continuation)
{
	const TUniquePtr<FVoxelTaskContextStrongRef> ContextStrongRef = ContextWeakRef.Pin();
	if (!ContextStrongRef)
	{
		return;
	}
	FVoxelTaskContext& Context = ContextStrongRef->Context;

	ON_SCOPE_EXIT
	{
		if (Continuation)
		{
			checkVoxelSlow(IsComplete());
			Continuation->Execute(Context, *this);
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
		VOXEL_SCOPE_LOCK(Context.CriticalSection);
		KeepAliveIndex = Context.PromisesToKeepAlive_RequiresLock.Add(AsShared());
	}

	checkVoxelSlow(!Continuation->NextContinuation);
	Continuation->NextContinuation = MoveTemp(Continuation_RequiresLock);

	checkVoxelSlow(!Continuation_RequiresLock);
	Continuation_RequiresLock = MoveTemp(Continuation);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromiseState::SetImpl(FVoxelTaskContext& Context)
{
	checkVoxelSlow(!bIsComplete.Get());
	bIsComplete.Set(true);

	ON_SCOPE_EXIT
	{
		Context.NumPromises.Decrement();

		if (KeepAliveIndex != -1)
		{
			VOXEL_SCOPE_LOCK(Context.CriticalSection);
			Context.PromisesToKeepAlive_RequiresLock.RemoveAt(KeepAliveIndex);

			KeepAliveIndex = -1;
		}

		if (StackIndex != -1)
		{
			VOXEL_SCOPE_LOCK(Context.CriticalSection);
			Context.StackFrames_RequiresLock.RemoveAt(StackIndex);

			StackIndex = -1;
		}
	};

	VOXEL_SCOPE_LOCK_ATOMIC(bIsLocked);

	TUniquePtr<FContinuation> Continuation = MoveTemp(Continuation_RequiresLock);
	while (Continuation)
	{
		Continuation->Execute(Context, *this);
		Continuation = MoveTemp(Continuation->NextContinuation);
	}
}