// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelEditorMinimal.h"

VOXEL_DEFAULT_MODULE(VoxelCoreEditor);

#if 0
VOXEL_RUN_ON_STARTUP_EDITOR(LogRedirects)
{
	FString String;
	for (TObjectIterator<UField> ClassIt; ClassIt; ++ClassIt)
	{
		UField* Field = *ClassIt;
		if (Field->GetOuter()->GetName() == "/Script/VoxelRaytracedCubic")
		{
			const FString Prefix =
				Cast<UScriptStruct>(Field)
				? "StructRedirects"
				: Cast<UClass>(Field)
				? "ClassRedirects"
				: Cast<UEnum>(Field)
				? "EnumRedirects"
				: "";
			if (!Prefix.IsEmpty())
			{
				String += FString::Printf(TEXT("+%s=(OldName=\"/Script/Voxel.%s\", NewName=\"/Script/VoxelRaytracedCubic.%s\")\n"), *Prefix, *Field->GetName(), *Field->GetName());
			}
		}
	}
	UE_DEBUG_BREAK();
}
#endif