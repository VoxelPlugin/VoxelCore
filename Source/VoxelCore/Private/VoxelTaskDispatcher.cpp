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