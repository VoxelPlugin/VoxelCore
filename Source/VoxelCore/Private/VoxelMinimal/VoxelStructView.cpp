// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "UObject/StructOnScope.h"
#include "VoxelMinimal/VoxelInstancedStruct.h"

FConstVoxelStructView::FConstVoxelStructView(const FStructOnScope& StructOnScope)
{
	ScriptStruct = ConstCast(CastChecked<UScriptStruct>(StructOnScope.GetStruct(), ECastCheckedType::NullAllowed));
	StructMemory = StructOnScope.GetStructMemory();
}

FConstVoxelStructView::FConstVoxelStructView(const FVoxelInstancedStruct& InstancedStruct)
	: FConstVoxelStructView(InstancedStruct.GetScriptStruct(), InstancedStruct.GetStructMemory())
{
}

FSharedVoidRef FConstVoxelStructView::MakeSharedCopy() const
{
	return MakeSharedStruct(GetScriptStruct(), GetStructMemory());
}

FVoxelInstancedStruct FConstVoxelStructView::MakeInstancedStruct() const
{
	FVoxelInstancedStruct Result;
	Result.InitializeAs(GetScriptStruct(), GetStructMemory());
	return Result;
}

bool FConstVoxelStructView::Identical(const FConstVoxelStructView Other) const
{
	checkVoxelSlow(IsValid());
	checkVoxelSlow(Other.IsValid());
	checkVoxelSlow(GetScriptStruct() == Other.GetScriptStruct());

	// Check that we have something to check
	checkVoxelSlow(EnumHasAllFlags(GetScriptStruct()->StructFlags, STRUCT_IdenticalNative) || GetScriptStruct()->PropertyLink);

	return GetScriptStruct()->CompareScriptStruct(Other.GetStructMemory(), GetStructMemory(), PPF_None);
}

void FConstVoxelStructView::CopyTo(const FVoxelStructView Other) const
{
	checkVoxelSlow(IsValid());
	checkVoxelSlow(Other.IsValid());
	checkVoxelSlow(GetScriptStruct() == Other.GetScriptStruct());

	if (GetScriptStruct()->StructFlags & STRUCT_CopyNative)
	{
		checkVoxelSlow(!(GetScriptStruct()->StructFlags & STRUCT_IsPlainOldData));

		if (!ensureVoxelSlow(GetScriptStruct()->GetCppStructOps()->Copy(Other.GetStructMemory(), GetStructMemory(), 1)))
		{
			GetScriptStruct()->CopyScriptStruct(Other.GetStructMemory(), GetStructMemory());
		}
	}
	else if (GetScriptStruct()->StructFlags & STRUCT_IsPlainOldData)
	{
		FMemory::Memcpy(Other.GetStructMemory(), GetStructMemory(), GetScriptStruct()->GetStructureSize());
	}
	else
	{
		// Check that we have something to copy
		checkVoxelSlow(GetScriptStruct()->PropertyLink);

		GetScriptStruct()->CopyScriptStruct(Other.GetStructMemory(), GetStructMemory());
	}
}

void FConstVoxelStructView::CheckSlow() const
{
	if (ScriptStruct &&
		ScriptStruct->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
	{
		check(ScriptStruct == static_cast<const FVoxelVirtualStruct*>(StructMemory)->GetStruct());
	}
}