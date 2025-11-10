// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelPropertyDiffing.h"
#include "UObject/StructOnScope.h"

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

TVoxelArray<FString> FConstVoxelStructView::GetChanges(const FConstVoxelStructView New) const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsValid());
	check(New.IsValid());
	check(GetScriptStruct() == New.GetScriptStruct());
	ensure(!(GetScriptStruct()->StructFlags & STRUCT_IdenticalNative));

	TVoxelArray<FString> Result;
	for (const FProperty& Property : GetStructProperties(GetScriptStruct()))
	{
		for (int32 Index = 0; Index < Property.ArrayDim; Index++)
		{
			FVoxelPropertyDiffing::Traverse(
				Property,
				Property.GetName() + (Property.ArrayDim == 1 ? FString() : FString::Printf(TEXT("[%d]"), Index)),
				Property.ContainerPtrToValuePtr<void>(GetStructMemory(), Index),
				Property.ContainerPtrToValuePtr<void>(New.GetStructMemory(), Index),
				Result);
		}
	}
	return Result;
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