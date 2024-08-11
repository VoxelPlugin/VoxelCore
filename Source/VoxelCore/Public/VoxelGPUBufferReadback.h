// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

class VOXELCORE_API FVoxelGPUBufferReadback : public TSharedFromThis<FVoxelGPUBufferReadback>
{
public:
	static TSharedRef<FVoxelGPUBufferReadback> Create(
		FRHICommandList& RHICmdList,
		FRHIBuffer* SourceBuffer);

	bool IsReady() const;

	TConstVoxelArrayView<uint8> Lock();
	void Unlock();

public:
	template<typename T>
	TVoxelArray<T> AsArray()
	{
		VOXEL_FUNCTION_COUNTER();

		const TConstVoxelArrayView<uint8> Data = Lock();
		ON_SCOPE_EXIT
		{
			Unlock();
		};

		if (!ensure(Data.Num() % sizeof(T) == 0))
		{
			return {};
		}

		return TVoxelArray<T>(Data.ReinterpretAs<T>());
	}
	template<typename T>
	T As()
	{
		const TConstVoxelArrayView<uint8> Data = Lock();
		ON_SCOPE_EXIT
		{
			Unlock();
		};

		return FromByteVoxelArrayView<T>(Data);
	}

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