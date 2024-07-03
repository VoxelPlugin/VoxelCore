// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskDispatcher.h"

class VOXELCORE_API FVoxelDefaultTaskDispatcherSingleton : public FVoxelSingleton
{
public:
	class FTaskDispatcher : public IVoxelTaskDispatcher
	{
		//~ Begin IVoxelTaskDispatcher Interface
		virtual void Dispatch(
			const EVoxelFutureThread Thread,
			TVoxelUniqueFunction<void()> Lambda) override
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
		//~ End IVoxelTaskDispatcher Interface
	};
	const TSharedRef<FTaskDispatcher> TaskDispatcher = MakeVoxelShared<FTaskDispatcher>();
};
FVoxelDefaultTaskDispatcherSingleton* GVoxelDefaultTaskDispatcherSingleton = new FVoxelDefaultTaskDispatcherSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

	return GetDefault();
}

IVoxelTaskDispatcher& FVoxelTaskDispatcherScope::GetDefault()
{
	return *GVoxelDefaultTaskDispatcherSingleton->TaskDispatcher;
}