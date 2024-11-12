// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskDispatcherInterface.h"
#include "VoxelGlobalTaskDispatcher.h"

FVoxelTaskDispatcherKeepAliveRef::~FVoxelTaskDispatcherKeepAliveRef()
{
	const TSharedPtr<IVoxelTaskDispatcher> Dispatcher = WeakDispatcher.Pin();
	if (!Dispatcher)
	{
		return;
	}

	VOXEL_SCOPE_LOCK(Dispatcher->CriticalSection);
	Dispatcher->PromisesToKeepAlive_RequiresLock.RemoveAt(Index);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_DEBUG
void IVoxelTaskDispatcher::CheckOwnsFuture(const FVoxelFuture& Future) const
{
	ensure(Future.PromiseState->Dispatcher_DebugOnly == AsWeak());
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture IVoxelTaskDispatcher::Wrap(
	const FVoxelFuture& Other,
	IVoxelTaskDispatcher& OtherDispatcher)
{
	VOXEL_FUNCTION_COUNTER();

	OtherDispatcher.CheckOwnsFuture(Other);

	FVoxelTaskDispatcherScope ThisScope(*this);

	if (Other.IsComplete())
	{
		return FVoxelFuture::Done();
	}

	const FVoxelPromise Promise;

	{
		FVoxelTaskDispatcherScope OtherScope(OtherDispatcher);

		Other.Then_AnyThread(MakeWeakPtrLambda(this, [this, Promise]
		{
			// Do a Dispatch(AsyncThread) to ensure this task dispatcher fully owns the lifetime of what Promise.Set will trigger
			Dispatch(EVoxelFutureThread::AsyncThread, [this, Promise]
			{
				checkVoxelSlow(&FVoxelTaskDispatcherScope::Get() == this);

				Promise.Set();
			});
		}));
	}

	return Promise.GetFuture();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelTaskDispatcherKeepAliveRef> IVoxelTaskDispatcher::AddRef(const TSharedRef<FVoxelPromiseState>& Promise)
{
	int32 Index;
	{
		VOXEL_SCOPE_LOCK(CriticalSection);
		Index = PromisesToKeepAlive_RequiresLock.Add(Promise);
	}
	return MakeVoxelShareable(new(GVoxelMemory) FVoxelTaskDispatcherKeepAliveRef(AsShared(), Index));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const uint32 GVoxelTaskDispatcherScopeTLS = FPlatformTLS::AllocTlsSlot();

FVoxelTaskDispatcherScope::FVoxelTaskDispatcherScope(IVoxelTaskDispatcher& Dispatcher)
	: Dispatcher(Dispatcher)
	, PreviousTLS(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS))
{
	FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, &Dispatcher);
}

FVoxelTaskDispatcherScope::~FVoxelTaskDispatcherScope()
{
	ensure(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS) == &Dispatcher);
	FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, PreviousTLS);
}

IVoxelTaskDispatcher& FVoxelTaskDispatcherScope::Get()
{
	if (IVoxelTaskDispatcher* Dispatcher = static_cast<IVoxelTaskDispatcher*>(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS)))
	{
		return *Dispatcher;
	}

	return GetGlobal();
}

IVoxelTaskDispatcher& FVoxelTaskDispatcherScope::GetGlobal()
{
	static const TSharedRef<FVoxelGlobalTaskDispatcher> TaskDispatcher = MakeVoxelShared<FVoxelGlobalTaskDispatcher>();

	return *TaskDispatcher;
}