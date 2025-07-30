// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "Containers/ResourceArray.h"

struct FVoxelResourceArrayRef : public FResourceArrayInterface
{
	const void* const Data;
	const uint32 DataSize;

	template<typename ArrayType>
	FVoxelResourceArrayRef(const ArrayType& Array)
		: Data(GetData(Array))
		, DataSize(GetNum(Array) * sizeof(*GetData(Array)))
	{
	}

	//~ Begin FResourceArrayInterface Interface
	virtual const void* GetResourceData() const override
	{
		return Data;
	}
	virtual uint32 GetResourceDataSize() const override
	{
		return DataSize;
	}

	virtual void Discard() override
	{
	}
	virtual bool IsStatic() const override
	{
		return true;
	}
	virtual bool GetAllowCPUAccess() const override
	{
		return false;
	}
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override
	{
	}
	//~ End FResourceArrayInterface Interface
};