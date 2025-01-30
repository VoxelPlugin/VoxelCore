// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelInstancedStructDataProvider.h"

bool FVoxelInstancedStructDataProvider::IsValid() const
{
	VOXEL_FUNCTION_COUNTER();

	bool bHasValidData = false;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](FVoxelInstancedStruct&)
	{
		bHasValidData = true;
	});
	return bHasValidData;
}

const UStruct* FVoxelInstancedStructDataProvider::GetBaseStructure() const
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSet<UScriptStruct*> Structs;
	FVoxelEditorUtilities::ForeachData<FVoxelInstancedStruct>(StructProperty, [&](const FVoxelInstancedStruct& InstancedStruct)
	{
		Structs.Add(InstancedStruct.GetScriptStruct());
	});

	if (Structs.Num() == 0)
	{
		return nullptr;
	}

	const UScriptStruct* BaseStruct = Structs.GetFirstValue();
	for (const UScriptStruct* Struct : Structs)
	{
		if (!Struct)
		{
			// No common struct
			return nullptr;
		}

		while (!Struct->IsChildOf(BaseStruct))
		{
			BaseStruct = Cast<UScriptStruct>(BaseStruct->GetSuperStruct());

			if (!BaseStruct)
			{
				return nullptr;
			}
		}
	}

	for (const UScriptStruct* Struct : Structs)
	{
		ensure(Struct->IsChildOf(BaseStruct));
	}

	return BaseStruct;
}

void FVoxelInstancedStructDataProvider::GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const
{
	VOXEL_FUNCTION_COUNTER();
	ensure(OutInstances.Num() == 0);

	const UScriptStruct* BaseStructure = Cast<UScriptStruct>(ExpectedBaseStructure);

	// The returned instances need to be compatible with base structure.
	// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
	FVoxelEditorUtilities::ForeachDataPtr<FVoxelInstancedStruct>(StructProperty, [&](FVoxelInstancedStruct* InstancedStruct)
	{
		if (!BaseStructure ||
			!InstancedStruct->IsA(BaseStructure))
		{
			OutInstances.Add(nullptr);
			return;
		}

		OutInstances.Add(MakeShared<FStructOnScope>(
			InstancedStruct->GetScriptStruct(),
			static_cast<uint8*>(InstancedStruct->GetStructMemory())));
	});

	TArray<UPackage*> Packages;
	StructProperty->GetOuterPackages(Packages);

	if (!ensure(Packages.Num() == OutInstances.Num()))
	{
		return;
	}

	for (int32 Index = 0; Index < OutInstances.Num(); Index++)
	{
		const TSharedPtr<FStructOnScope> Instance = OutInstances[Index];
		if (!Instance)
		{
			continue;
		}

		Instance->SetPackage(Packages[Index]);
	}
}

bool FVoxelInstancedStructDataProvider::IsPropertyIndirection() const
{
	return true;
}

uint8* FVoxelInstancedStructDataProvider::GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const
{
	if (!ensureVoxelSlow(ParentValueAddress))
	{
		return nullptr;
	}

	FVoxelInstancedStruct& InstancedStruct = *reinterpret_cast<FVoxelInstancedStruct*>(ParentValueAddress);
	if (ExpectedBaseStructure &&
		InstancedStruct.GetScriptStruct() &&
		InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
	{
		return static_cast<uint8*>(InstancedStruct.GetStructMemory());
	}

	return nullptr;
}