// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "JsonObjectConverter.h"

DEFINE_VOXEL_INSTANCE_COUNTER(FVoxelVirtualStruct);

#if DO_CHECK
VOXEL_RUN_ON_STARTUP_GAME()
{
	for (UScriptStruct* Struct : GetDerivedStructs<FVoxelVirtualStruct>())
	{
		const TSharedRef<FVoxelVirtualStruct> Instance = MakeSharedStruct<FVoxelVirtualStruct>(Struct);

		ensureAlwaysMsgf(Instance->GetStruct() == Struct, TEXT("Missing %s() in %s"),
			*Instance->Internal_GetMacroName(),
			*Struct->GetStructCPPName());
	}
}
#endif

FString FVoxelVirtualStruct::Internal_GetMacroName() const
{
	return "GENERATED_VIRTUAL_STRUCT_BODY";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelVirtualStruct> FVoxelVirtualStruct::MakeSharedCopy_Generic() const
{
	return MakeSharedStruct(GetStruct(), this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FJsonObject> FVoxelVirtualStruct::SaveToJson(
	const EPropertyFlags CheckFlags,
	const EPropertyFlags SkipFlags) const
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	verify(FJsonObjectConverter::UStructToJsonObject(
		GetStruct(),
		this,
		JsonObject,
		CheckFlags,
		SkipFlags));
	return JsonObject;
}

bool FVoxelVirtualStruct::LoadFromJson(
	const TSharedRef<FJsonObject>& JsonObject,
	const bool bStrictMode,
	const EPropertyFlags CheckFlags,
	const EPropertyFlags SkipFlags)
{
	VOXEL_FUNCTION_COUNTER();

	return FJsonObjectConverter::JsonObjectToUStruct(
		JsonObject,
		GetStruct(),
		this,
		CheckFlags,
		SkipFlags,
		bStrictMode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelVirtualStruct::Equals_UPropertyOnly(
	const FVoxelVirtualStruct& Other,
	const bool bIgnoreTransient) const
{
	if (GetStruct() != Other.GetStruct())
	{
		return false;
	}

	for (const FProperty& Property : GetStructProperties(GetStruct()))
	{
		if (bIgnoreTransient &&
			Property.HasAllPropertyFlags(CPF_Transient))
		{
			continue;
		}

		if (!Property.Identical_InContainer(this, &Other))
		{
			return false;
		}
	}

	return true;
}