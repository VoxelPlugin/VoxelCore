// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Bulk/VoxelBulkHint.h"

class FVoxelBulkHintManager : public FVoxelSingleton
{
public:
	TVoxelArray<const UScriptStruct*> IndexToStruct;
	TVoxelMap<const UScriptStruct*, uint8> StructToIndex;

	virtual void Initialize() override
	{
		for (const UScriptStruct* Struct : GetDerivedStructs<FVoxelBulkHintData>())
		{
			IndexToStruct.Add(Struct);
		}
		IndexToStruct.Sort([](const UScriptStruct& A, const UScriptStruct& B)
		{
			return A.GetName() < B.GetName();
		});

		IndexToStruct.Insert(nullptr, 0);
		for (int32 Index = 0; Index < IndexToStruct.Num(); Index++)
		{
			StructToIndex.Add_EnsureNew(IndexToStruct[Index], Index);
		}
	}
};
FVoxelBulkHintManager* GVoxelBulkHintManager = new FVoxelBulkHintManager();

void FVoxelBulkHint::Serialize(FArchive& Ar)
{
	uint8 Index = 0;
	if (Ar.IsSaving())
	{
		if (Data)
		{
			Index = GVoxelBulkHintManager->StructToIndex.FindChecked(Data->GetStruct());
		}
	}

	Ar << Index;

	if (Ar.IsLoading())
	{
		if (const UScriptStruct* Struct = GVoxelBulkHintManager->IndexToStruct[Index])
		{
			Data = MakeSharedStruct<FVoxelBulkHintData>(Struct);
		}
	}

	if (Data)
	{
		ConstCast(*Data).Serialize(Ar);
	}
}