// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"
#include "VoxelPromiseState.h"

DEFINE_VOXEL_INSTANCE_COUNTER(IVoxelPromiseState);

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::New(
	FVoxelTaskContext* ContextOverride,
	const bool bWithValue)
{
	return new FVoxelPromiseState(ContextOverride, bWithValue);
}

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::New(const FSharedVoidRef& Value)
{
	return new FVoxelPromiseState(Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelPromiseState::Set()
{
	static_cast<FVoxelPromiseState*>(this)->Set();
}

void IVoxelPromiseState::Set(const FSharedVoidRef& NewValue)
{
	static_cast<FVoxelPromiseState*>(this)->Set(NewValue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_DEBUG
void IVoxelPromiseState::CheckCanAddContinuation(const FVoxelFuture& Future)
{
	check(!Future.IsComplete());

	const FVoxelTaskContextWeakRef ThisContext = static_cast<FVoxelPromiseState&>(*this).ContextWeakRef;
	const FVoxelTaskContextWeakRef OtherContext = static_cast<FVoxelPromiseState&>(*Future.PromiseState).ContextWeakRef;

	if (ThisContext == OtherContext)
	{
		return;
	}

	const TUniquePtr<FVoxelTaskContextStrongRef> ThisContextPtr = ThisContext.Pin();
	const TUniquePtr<FVoxelTaskContextStrongRef> OtherContextPtr = OtherContext.Pin();

	if (!ThisContextPtr)
	{
		return;
	}

	// If we can cancel tasks we cannot have a future in a different context depend on us, as we will never complete if cancelled
	// That other future, being in a different context, won't be cancelled and will be stuck
	check(
		&ThisContextPtr->Context == GVoxelGlobalTaskContext ||
		&ThisContextPtr->Context == GVoxelSynchronousTaskContext);
}
#endif

void IVoxelPromiseState::AddContinuation(const FVoxelFuture& Future)
{
	CheckCanAddContinuation(Future);
	static_cast<FVoxelPromiseState&>(*this).AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Future));
}

void IVoxelPromiseState::AddContinuation(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Continuation)
{
	static_cast<FVoxelPromiseState*>(this)->AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Thread, MoveTemp(Continuation)));
}

void IVoxelPromiseState::AddContinuation(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void(const FSharedVoidRef&)> Continuation)
{
	static_cast<FVoxelPromiseState*>(this)->AddContinuation(MakeUnique<FVoxelPromiseState::FContinuation>(Thread, MoveTemp(Continuation)));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IVoxelPromiseState::Destroy()
{
	delete static_cast<FVoxelPromiseState*>(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture::FVoxelFuture(const TConstVoxelArrayView<FVoxelFuture> Futures)
{
	VOXEL_FUNCTION_COUNTER_NUM(Futures.Num(), 16);

	if (Futures.Num() == 0)
	{
		return;
	}

	PromiseState = IVoxelPromiseState::New(nullptr, false);

	const TSharedRef<FVoxelCounter32> Counter = MakeShared<FVoxelCounter32>(Futures.Num());

	for (const FVoxelFuture& Future : Futures)
	{
		if (Future.IsComplete())
		{
			if (Counter->Decrement_ReturnNew() == 0)
			{
				PromiseState->Set();
			}
			continue;
		}

		Future.PromiseState->CheckCanAddContinuation(*this);
		Future.PromiseState->AddContinuation(EVoxelFutureThread::AnyThread, [Counter, PromiseState = PromiseState]
		{
			if (Counter->Decrement_ReturnNew() == 0)
			{
				PromiseState->Set();
			}
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelFuture::ExecuteImpl(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
	FVoxelTaskScope::GetContext().Dispatch(Thread, MoveTemp(Lambda));
}