// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskDispatcher.h"

const uint32 GVoxelTaskDispatcherScopeTLS = FPlatformTLS::AllocTlsSlot();

FVoxelTaskDispatcherScope::FVoxelTaskDispatcherScope(const TSharedRef<IVoxelTaskDispatcher>& Dispatcher)
	: Dispatcher(Dispatcher)
	, PreviousTLS(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS))
{
	FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, this);
}

FVoxelTaskDispatcherScope::~FVoxelTaskDispatcherScope()
{
	ensure(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS) == this);
	FPlatformTLS::SetTlsValue(GVoxelTaskDispatcherScopeTLS, PreviousTLS);
}

TSharedPtr<IVoxelTaskDispatcher> FVoxelTaskDispatcherScope::Get()
{
	const FVoxelTaskDispatcherScope* Scope = static_cast<FVoxelTaskDispatcherScope*>(FPlatformTLS::GetTlsValue(GVoxelTaskDispatcherScopeTLS));
	if (!Scope)
	{
		return nullptr;
	}
	return Scope->Dispatcher;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDefaultTaskDispatcher::Dispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
	StaticDispatch(Thread, MoveTemp(Lambda));
}

void FVoxelDefaultTaskDispatcher::StaticDispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
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