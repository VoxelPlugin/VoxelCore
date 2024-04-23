// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskDispatcher.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPromiseState::FVoxelPromiseState()
{
	if (const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = FVoxelTaskDispatcherScope::Get())
	{
		Dispatcher->PrivateNumPromises.Increment();
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
		ThisPtr_RequiresLock.Reset();
	}

	if (const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = FVoxelTaskDispatcherScope::Get())
	{
		for (auto& It : ThreadToContinuation)
		{
			Dispatcher->Dispatch(It.Key, [Continuation = MoveTemp(It.Value), Value]
			{
				Continuation(Value);
			});
		}

		Dispatcher->PrivateNumPromises.Decrement();
	}
	else
	{
		for (auto& It : ThreadToContinuation)
		{
			StaticDispatch(It.Key, [Continuation = MoveTemp(It.Value), Value]
			{
				Continuation(Value);
			});
		}
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
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		if (!bIsComplete.Get())
		{
			ThreadToContinuation_RequiresLock.Add({ Thread, MoveTemp(Continuation) });

			if (!ThisPtr_RequiresLock)
			{
				// Ensure we're kept alive until all delegates are fired
				ThisPtr_RequiresLock = AsShared();
			}
			return;
		}
	}

	checkVoxelSlow(IsComplete());
	checkVoxelSlow(Value_RequiresLock.IsValid());

	if (const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = FVoxelTaskDispatcherScope::Get())
	{
		Dispatcher->Dispatch(Thread, [Continuation = MoveTemp(Continuation), Value = Value_RequiresLock.ToSharedRef()]
		{
			Continuation(Value);
		});
	}
	else
	{
		StaticDispatch(Thread, [Continuation = MoveTemp(Continuation), Value = Value_RequiresLock.ToSharedRef()]
		{
			Continuation(Value);
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType>
FORCEINLINE void FVoxelPromiseState::StaticDispatch(
	const EVoxelFutureThread Thread,
	LambdaType Lambda)
{
	switch (Thread)
	{
	default: VOXEL_ASSUME(false);
	case EVoxelFutureThread::AnyThread:
	{
		Lambda();
	}
	break;
	case EVoxelFutureThread::GameThread:
	{
		RunOnGameThread(MoveTemp(Lambda));
	}
	break;
	case EVoxelFutureThread::RenderThread:
	{
		VOXEL_ENQUEUE_RENDER_COMMAND(Future)([Lambda = MoveTemp(Lambda)](FRHICommandListImmediate& RHICmdList)
		{
			Lambda();
		});
	}
	break;
	case EVoxelFutureThread::AsyncThread:
	{
		AsyncBackgroundTaskImpl(MoveTemp(Lambda));
	}
	break;
	}
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
		Future.Then_AnyThread([=]
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

void FVoxelPromise::Set() const
{
	PromiseState->Set(MakeSharedVoid());
}