// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskDispatcher.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPromiseState::FVoxelPromiseState()
{
	IVoxelTaskDispatcher& Dispatcher = FVoxelTaskDispatcherScope::Get();

	if (FVoxelCounter32* NumPromisesPtr = Dispatcher.NumPromisesPtr)
	{
		NumPromisesPtr->Increment();

#if VOXEL_DEBUG
		VOXEL_SCOPE_LOCK(Dispatcher.StacksCriticalSection);
		Dispatcher.PromiseStateToStackFrames_RequiresLock.Add_EnsureNew(this, FVoxelUtilities::GetStackFrames(2));
#endif
	}
}

FVoxelPromiseState::~FVoxelPromiseState()
{
}

void FVoxelPromiseState::Set(const FSharedVoidRef& Value)
{
	FThreadToContinuation ThreadToContinuation;
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		checkVoxelSlow(!Value_RequiresLock);
		Value_RequiresLock = Value;

		checkVoxelSlow(!bIsComplete.Get());
		bIsComplete.Set(true);

		ThreadToContinuation = MoveTemp(ThreadToContinuation_RequiresLock);

		// No need to keep ourselves alive anymore
		KeepAliveRef_RequiresLock.Reset();
	}

	IVoxelTaskDispatcher& Dispatcher = FVoxelTaskDispatcherScope::Get();

	for (auto& It : ThreadToContinuation)
	{
		Dispatcher.Dispatch(It.Key, [Continuation = MoveTemp(It.Value), Value]
		{
			Continuation(Value);
		});
	}

	if (Dispatcher.NumPromisesPtr)
	{
		Dispatcher.NumPromisesPtr->Decrement();

#if VOXEL_DEBUG
		VOXEL_SCOPE_LOCK(Dispatcher.StacksCriticalSection);
		ensure(Dispatcher.PromiseStateToStackFrames_RequiresLock.Remove(this));
#endif
	}
}

void FVoxelPromiseState::Set(const FVoxelFuture& Future)
{
	if (Future.IsComplete())
	{
		Set(Future.PromiseState->Value_RequiresLock.ToSharedRef());
		return;
	}

	Future.PromiseState->AddContinuation(EVoxelFutureThread::AnyThread, [This = AsShared()](const FSharedVoidRef& Value)
	{
		This->Set(Value);
	});
}

void FVoxelPromiseState::AddContinuation(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void(const FSharedVoidRef&)> Continuation)
{
	IVoxelTaskDispatcher& Dispatcher = FVoxelTaskDispatcherScope::Get();

	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (!bIsComplete.Get())
		{
			ThreadToContinuation_RequiresLock.Add({ Thread, MoveTemp(Continuation) });

			if (!KeepAliveRef_RequiresLock)
			{
				// Ensure we're kept alive until all delegates are fired
				KeepAliveRef_RequiresLock = Dispatcher.AddRef(AsShared());
			}
			return;
		}
	}

	checkVoxelSlow(IsComplete());
	checkVoxelSlow(Value_RequiresLock.IsValid());

	Dispatcher.Dispatch(Thread, [Continuation = MoveTemp(Continuation), Value = Value_RequiresLock.ToSharedRef()]
	{
		Continuation(Value);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture::FVoxelFuture(const TConstVoxelArrayView<FVoxelFuture> Futures)
{
	VOXEL_FUNCTION_COUNTER_NUM(Futures.Num(), 16);

	if (Futures.Num() == 0)
	{
		const FVoxelPromise Promise;
		Promise.Set();
		*this = Promise.GetFuture();
		return;
	}

	const FVoxelPromise Promise;

	const TSharedRef<FVoxelCounter32> Counter = MakeVoxelShared<FVoxelCounter32>();
	Counter->Set(Futures.Num());

	for (const FVoxelFuture& Future : Futures)
	{
		Future.PromiseState->AddContinuation(EVoxelFutureThread::AnyThread, [=](const FSharedVoidRef&)
		{
			if (Counter->Decrement_ReturnNew() == 0)
			{
				Promise.Set();
			}
		});
	}

	*this = Promise.GetFuture();
}

FVoxelFuture FVoxelFuture::Done()
{
	const FVoxelPromise Promise;
	Promise.Set();
	return Promise.GetFuture();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelFuture::ExecuteImpl(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
	FVoxelTaskDispatcherScope::Get().Dispatch(Thread, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromise::Set() const
{
	PromiseState->Set(MakeSharedVoid());
}

void FVoxelPromise::Set(const FVoxelFuture& Future) const
{
	PromiseState->Set(Future);
}