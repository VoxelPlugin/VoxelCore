// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelTaskDispatcherInterface.h"

VOXEL_CONSOLE_COMMAND(
	"voxel.EnablePromiseTracking",
	"")
{
	FVoxelPromiseState::EnableTracking();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelPromiseState);

#if !UE_BUILD_SHIPPING
struct FVoxelPromiseTracking
{
public:
	bool bEnable = false;
	FVoxelCriticalSection CriticalSection;
	TVoxelSparseArray<FVoxelStackFrames> StackFrames_RequiresLock;

public:
	FORCENOINLINE int32 GetStackIndex()
	{
		const FVoxelStackFrames StackFrames = FVoxelUtilities::GetStackFrames(4);

		VOXEL_SCOPE_LOCK(CriticalSection);
		return StackFrames_RequiresLock.Add(StackFrames);
	}
	FORCENOINLINE void RemoveStackIndex(const int32 StackIndex)
	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		StackFrames_RequiresLock.RemoveAt(StackIndex);
	}

public:
	void DumpToLog()
	{
		VOXEL_FUNCTION_COUNTER();

		if (!bEnable)
		{
			return;
		}

		FPlatformStackWalk::InitStackWalking();

		VOXEL_SCOPE_LOCK(CriticalSection);

		TVoxelMap<FVoxelStackFrames, int32> StackFramesToCount;
		StackFramesToCount.Reserve(StackFrames_RequiresLock.Num());

		for (const FVoxelStackFrames& StackFrames : StackFrames_RequiresLock)
		{
			StackFramesToCount.FindOrAdd(StackFrames)++;
		}

		StackFramesToCount.ValueSort([](const int32 A, const int32 B)
		{
			return A > B;
		});

		LOG_VOXEL(Log, "%d unfulfilled promises", StackFrames_RequiresLock.Num());

		for (const auto& It : StackFramesToCount)
		{
			LOG_VOXEL(Log, "-----------------------------------");
			LOG_VOXEL(Log, "x%d:", It.Value);

			for (int32 Index = 0; Index < It.Key.Num(); Index++)
			{
				void* Address = It.Key[Index];
				if (!Address)
				{
					continue;
				}

				ANSICHAR HumanReadableString[4096];
				FPlatformStackWalk::ProgramCounterToHumanReadableString(Index, uint64(Address), HumanReadableString, 4096);

				LOG_VOXEL(Log, "%p: %S", Address, HumanReadableString);
			}
		}
	}
};
FVoxelPromiseTracking GVoxelPromiseTracking;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPromiseState::FVoxelPromiseState()
{
#if !UE_BUILD_SHIPPING
	if (GVoxelPromiseTracking.bEnable)
	{
		DebugStackIndex = GVoxelPromiseTracking.GetStackIndex();
	}
#endif

	IVoxelTaskDispatcher& Dispatcher = FVoxelTaskDispatcherScope::Get();

	if (FVoxelCounter32* NumPromisesPtr = Dispatcher.NumPromisesPtr)
	{
		NumPromisesPtr->Increment();
	}

#if VOXEL_DEBUG
	Dispatcher_DebugOnly = Dispatcher.AsWeak();
	DebugStackFrames = FVoxelUtilities::GetStackFrames(2);

	VOXEL_SCOPE_LOCK(Dispatcher.PromisesCriticalSection);
	Dispatcher.Promises_RequiresLock.Add_EnsureNew(this);
#endif
}

FVoxelPromiseState::~FVoxelPromiseState()
{
#if !UE_BUILD_SHIPPING
	if (DebugStackIndex != -1)
	{
		GVoxelPromiseTracking.RemoveStackIndex(DebugStackIndex);
		DebugStackIndex = -1;
	}
#endif

#if VOXEL_DEBUG
	// Don't check FVoxelTaskDispatcherScope::Get() in destructor, enforcing destructor scoping is way too messy
	// (eg single class could store futures from two different dispatchers)

	const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = Dispatcher_DebugOnly.Pin();
	if (!Dispatcher)
	{
		return;
	}

	ensure(IsComplete() || Dispatcher->IsExiting());

	VOXEL_SCOPE_LOCK(Dispatcher->PromisesCriticalSection);
	ensure(Dispatcher->Promises_RequiresLock.Remove(this));
#endif
}

void FVoxelPromiseState::Set(const FSharedVoidRef& Value)
{
#if VOXEL_DEBUG
	ensure(FVoxelTaskDispatcherScope::Get().AsWeak() == Dispatcher_DebugOnly);
#endif

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
	}

#if !UE_BUILD_SHIPPING
	if (DebugStackIndex != -1)
	{
		GVoxelPromiseTracking.RemoveStackIndex(DebugStackIndex);
		DebugStackIndex = -1;
	}
#endif
}

void FVoxelPromiseState::Set(const FVoxelFuture& Future)
{
#if VOXEL_DEBUG
	ensure(FVoxelTaskDispatcherScope::Get().AsWeak() == Dispatcher_DebugOnly);
#endif

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
#if VOXEL_DEBUG
	ensure(FVoxelTaskDispatcherScope::Get().AsWeak() == Dispatcher_DebugOnly);
#endif

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

void FVoxelPromiseState::EnableTracking()
{
#if !UE_BUILD_SHIPPING
	LOG_VOXEL(Log, "Enabling promise tracking");
	GVoxelPromiseTracking.bEnable = true;
#endif
}

void FVoxelPromiseState::DumpAllPromises()
{
#if !UE_BUILD_SHIPPING
	GVoxelPromiseTracking.DumpToLog();
#endif
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