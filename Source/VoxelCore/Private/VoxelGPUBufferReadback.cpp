// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelGPUBufferReadback.h"
#include "RHIGPUReadback.h"

TSharedRef<FVoxelGPUBufferReadback> FVoxelGPUBufferReadback::Create(
	FRHICommandList& RHICmdList,
	FRHIBuffer* SourceBuffer,
	int64 NumBytes)
{
	VOXEL_FUNCTION_COUNTER();
	check(SourceBuffer);

	if (NumBytes == -1)
	{
		NumBytes = SourceBuffer->GetSize();
	}

	const TSharedRef<FRHIGPUBufferReadback> Readback = MakeShared<FRHIGPUBufferReadback>(SourceBuffer->GetName() + TEXTVIEW(" Readback"));
	Readback->EnqueueCopy(RHICmdList, SourceBuffer, NumBytes);

	return MakeVoxelShareable_RenderThread(new(GVoxelMemory) FVoxelGPUBufferReadback(NumBytes, Readback));
}

bool FVoxelGPUBufferReadback::IsReady() const
{
	return Readback->IsReady();
}

TConstVoxelArrayView64<uint8> FVoxelGPUBufferReadback::Lock()
{
	VOXEL_FUNCTION_COUNTER();

	check(IsReady());
	return TConstVoxelArrayView64<uint8>(static_cast<const uint8*>(Readback->Lock(NumBytes)), NumBytes);
}

void FVoxelGPUBufferReadback::Unlock()
{
	VOXEL_FUNCTION_COUNTER();

	Readback->Unlock();
}