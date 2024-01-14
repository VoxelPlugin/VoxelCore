// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "RenderingThread.h"

template<typename T>
class TRefCountPtr_RenderThread
{
public:
	TRefCountPtr_RenderThread() = default;
	TRefCountPtr_RenderThread(T* Reference)
	{
		this->SetReference(Reference);
	}
	TRefCountPtr_RenderThread(const TRefCountPtr_RenderThread& Other)
	{
		this->SetReference(Other.Reference);
	}
	TRefCountPtr_RenderThread(TRefCountPtr_RenderThread&& Other)
	{
		Reference = Other.Reference;
		Other.Reference = nullptr;
	}
	TRefCountPtr_RenderThread(const TRefCountPtr<T>& Other)
	{
		this->SetReference(Other.GetReference());
	}
	~TRefCountPtr_RenderThread()
	{
		Reset();
	}

public:
	TRefCountPtr_RenderThread& operator=(const TRefCountPtr_RenderThread& Other)
	{
		this->SetReference(Other.Reference);
		return *this;
	}
	TRefCountPtr_RenderThread& operator=(TRefCountPtr_RenderThread&& Other)
	{
		Reset();

		Reference = Other.Reference;
		Other.Reference = nullptr;

		return *this;
	}

public:
	void SetReference(T* NewReference)
	{
		Reset();

		Reference = NewReference;

		if (Reference)
		{
			Reference->AddRef();
		}
	}
	void Reset()
	{
		if (!Reference)
		{
			return;
		}

		checkVoxelSlow(Reference->GetRefCount() >= 1);

		ENQUEUE_RENDER_COMMAND(TRefCountPtr_RenderThread)([Reference = Reference](FRHICommandList& RHICmdList)
		{
			checkVoxelSlow(Reference->GetRefCount() >= 1);
			Reference->Release();
		});

		Reference = nullptr;
	}

	FORCEINLINE bool IsValid() const
	{
		return Reference != nullptr;
	}
	FORCEINLINE T* Get() const
	{
		return Reference;
	}

	FORCEINLINE T* operator->() const
	{
		return Reference;
	}
	FORCEINLINE operator T*() const
	{
		return Reference;
	}
	FORCEINLINE operator bool() const
	{
		return IsValid();
	}

private:
	T* Reference = nullptr;
};