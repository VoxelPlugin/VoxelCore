// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "IStructureDataProvider.h"

class FVoxelInstancedStructDataProvider : public IStructureDataProvider
{
public:
	const TSharedRef<IPropertyHandle> StructProperty;

	explicit FVoxelInstancedStructDataProvider(const TSharedRef<IPropertyHandle>& StructProperty)
		: StructProperty(StructProperty)
	{
	}

	//~ Begin IStructureDataProvider Interface
	virtual bool IsValid() const override;
	virtual const UStruct* GetBaseStructure() const override;
	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override;
	virtual bool IsPropertyIndirection() const override;
	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const override;
	//~ End IStructureDataProvider Interface
};