// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPromiseState::~FVoxelPromiseState()
{
	ensure(IsComplete());
	ensure(ThreadToContinuation_RequiresLock.Num() == 0);
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

	for (auto& It : ThreadToContinuation)
	{
		Dispatch(It.Key, [Continuation = MoveTemp(It.Value), Value]
		{
			Continuation(Value);
		});
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

	Dispatch(Thread, [Continuation = MoveTemp(Continuation), Value = Value_RequiresLock.ToSharedRef()]
	{
		Continuation(Value);
	});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType>
FORCEINLINE void FVoxelPromiseState::Dispatch(
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
	case EVoxelFutureThread::VoxelThread:
	{
		AsyncVoxelTaskImpl(MoveTemp(Lambda));
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPromise::Set() const
{
	PromiseState->Set(MakeSharedVoid());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFuture<bool> operator&&(const TVoxelFuture<bool>& A, const TVoxelFuture<bool>& B)
{
	return A.Then_AnyThread([=](const bool bValueA)
	{
		return B.Then_AnyThread([=](const bool bValueB)
		{
			return bValueA && bValueB;
		});
	});
}