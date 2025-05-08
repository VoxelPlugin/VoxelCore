// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

// Prefer using FVoxelUtilities::Readback over this
class VOXELCORE_API FVoxelGPUBufferReadback : public TSharedFromThis<FVoxelGPUBufferReadback>
{
public:
	static TSharedRef<FVoxelGPUBufferReadback> Create(
		FRHICommandList& RHICmdList,
		FRHIBuffer* SourceBuffer,
		int64 NumBytes = -1);

	bool IsReady() const;

	TConstVoxelArrayView64<uint8> Lock();
	void Unlock();

private:
	const int64 NumBytes;
	const TSharedRef<FRHIGPUBufferReadback> Readback;

	FVoxelGPUBufferReadback(
		const int64 NumBytes,
		const TSharedRef<FRHIGPUBufferReadback>& Readback)
		: NumBytes(NumBytes)
		, Readback(Readback)
	{
	}
};