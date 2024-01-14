// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "C:/Program Files/RenderDoc/renderdoc_app.h"

RENDERDOC_API_1_1_2* GVoxelRenderDocAPI = []
{
	void* DllHandle = FPlatformProcess::GetDllHandle(TEXT("C:/Program Files/RenderDoc/renderdoc.dll"));
	check(DllHandle);

	const pRENDERDOC_GetAPI GetRenderDocAPI = static_cast<pRENDERDOC_GetAPI>(FPlatformProcess::GetDllExport(DllHandle, TEXT("RENDERDOC_GetAPI")));
	check(GetRenderDocAPI);

	RENDERDOC_API_1_1_2* RenderDocAPI = nullptr;
    verify(1 == GetRenderDocAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&RenderDocAPI)));
	check(RenderDocAPI);

	return RenderDocAPI;
}();

void StartRenderDocCapture()
{
	(*GVoxelRenderDocAPI->LaunchReplayUI)(true, nullptr);
	(*GVoxelRenderDocAPI->StartFrameCapture)(nullptr, nullptr);
}
void EndRenderDocCapture()
{
	(*GVoxelRenderDocAPI->EndFrameCapture)(nullptr, nullptr);
}