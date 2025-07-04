// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskContext.h"
#include "VoxelPromiseState.h"

DEFINE_VOXEL_INSTANCE_COUNTER(IVoxelPromiseState);

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::New_WithValue(FVoxelTaskContext* ContextOverride)
{
	FVoxelPromiseState* Result = new FVoxelPromiseState(ContextOverride);
	ConstCast(Result->bHasValue) = true;
	return Result;
}

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::New_WithoutValue(FVoxelTaskContext* ContextOverride)
{
	return new FVoxelPromiseState(ContextOverride);
}

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::New(const FSharedVoidRef& Value)
{
	return new FVoxelPromiseState(Value);
}

TVoxelRefCountPtr<IVoxelPromiseState> IVoxelPromiseState::NewWithDependencies(const int32 NumDependenciesLeft)
{
	checkVoxelSlow(NumDependenciesLeft > 0);

	FVoxelPromiseState* Result = new FVoxelPromiseState(nullptr);
	Result->NumDependencies.Set(NumDependenciesLeft);
	return Result;
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
	Initialize(Futures);
}

FVoxelFuture::FVoxelFuture(const TVoxelChunkedArray<FVoxelFuture>& Futures)
{
	Initialize(Futures);
}

template<typename ArrayType>
void FVoxelFuture::Initialize(const ArrayType& Futures)
{
	VOXEL_FUNCTION_COUNTER_NUM(Futures.Num(), 16);

	if (Futures.Num() == 0)
	{
		return;
	}

	checkVoxelSlow(!PromiseState);
	PromiseState = IVoxelPromiseState::NewWithDependencies(Futures.Num());

	for (const FVoxelFuture& Future : Futures)
	{
		Initialize_AddFuture(Future);
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