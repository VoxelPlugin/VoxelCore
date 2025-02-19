// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectThreadContext.h"

void FVoxelInstancedStruct::InitializeAs(UScriptStruct* NewScriptStruct, const void* NewStructMemory)
{
	*this = {};

	if (!NewScriptStruct)
	{
		// Null
		ensure(!NewStructMemory);
		return;
	}

	PrivateScriptStruct = NewScriptStruct;
	PrivateStructMemory = MakeSharedStruct(NewScriptStruct, NewStructMemory);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedStruct::Reset()
{
	*this = FVoxelInstancedStruct();
}

bool FVoxelInstancedStruct::NetSerialize(FArchive& Ar, UPackageMap& Map)
{
	if (Ar.IsSaving())
	{
		if (!IsValid())
		{
			FString PathName;
			Ar << PathName;
			return true;
		}

		FString PathName = GetScriptStruct()->GetPathName();
		Ar << PathName;

		GetScriptStruct()->SerializeItem(Ar, GetStructMemory(), nullptr);
		return true;
	}
	else if (Ar.IsLoading())
	{
		FString PathName;
		Ar << PathName;

		if (PathName.IsEmpty())
		{
			*this = {};
			return true;
		}

		UScriptStruct* NewScriptStruct = LoadObject<UScriptStruct>(nullptr, *PathName);
		if (!ensure(NewScriptStruct))
		{
			return false;
		}

		*this = FVoxelInstancedStruct(NewScriptStruct);
		GetScriptStruct()->SerializeItem(Ar, GetStructMemory(), nullptr);
		return true;
	}
	else
	{
		ensure(false);
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelInstancedStruct::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	using FVersion = DECLARE_VOXEL_VERSION
	(
		InitialVersion,
		StoreName
	);

	uint8 Version = FVersion::LatestVersion;
	Ar << Version;
	ensure(Version <= FVersion::LatestVersion);

	if (Ar.IsLoading())
	{
		FString StructPath;
		if (Version >= FVersion::StoreName)
		{
			Ar << StructPath;
		}

		{
			UScriptStruct* NewScriptStruct = nullptr;
			Ar << NewScriptStruct;

			if (!NewScriptStruct &&
				StructPath != TEXTVIEW("<null>"))
			{
				VOXEL_SCOPE_COUNTER("FindFirstObject");

				// Serializing structs directly doesn't seem to handle redirects properly
				const FCoreRedirectObjectName RedirectedName = FCoreRedirects::GetRedirectedName(
					ECoreRedirectFlags::Type_Struct,
					FCoreRedirectObjectName(StructPath),
					ECoreRedirectMatchFlags::AllowPartialMatch);

				NewScriptStruct = FindFirstObject<UScriptStruct>(*RedirectedName.ToString(), EFindFirstObjectOptions::EnsureIfAmbiguous);
			}

			if (NewScriptStruct)
			{
				Ar.Preload(NewScriptStruct);
			}

			if (NewScriptStruct != GetScriptStruct())
			{
				InitializeAs(NewScriptStruct);
			}
			ensure(GetScriptStruct() == NewScriptStruct);
		}

		int32 SerializedSize = 0;
		Ar << SerializedSize;

		if (!GetScriptStruct() && SerializedSize > 0)
		{
			LOG_VOXEL(Warning, "Struct %s not found. Archive: %s Callstack: \n%s",
				*StructPath,
				*FVoxelUtilities::GetArchivePath(Ar),
				*FVoxelUtilities::GetPrettyCallstack());

			Ar.Seek(Ar.Tell() + SerializedSize);
		}
		else if (GetScriptStruct())
		{
			const int64 Start = Ar.Tell();
			GetScriptStruct()->SerializeItem(Ar, GetStructMemory(), nullptr);
			ensure(Ar.Tell() - Start == SerializedSize);

			if (GetScriptStruct()->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
			{
				static_cast<FVoxelVirtualStruct*>(GetStructMemory())->PostSerialize();
			}

			if (GetScriptStruct() == StaticStructFast<FBodyInstance>())
			{
				static_cast<FBodyInstance*>(GetStructMemory())->LoadProfileData(false);
			}
		}
	}
	else if (
		Ar.IsSaving() ||
		// Make sure object references are accounted for
		// This is critical for FindReferences or example packaging to work
		Ar.IsObjectReferenceCollector() ||
		Ar.IsCountingMemory())
	{
		FString StructPath;
		if (GetScriptStruct())
		{
			StructPath = GetScriptStruct()->GetPathName();
		}
		else
		{
			StructPath = "<null>";
		}

		Ar << StructPath;
		Ar << PrivateScriptStruct;

		const int64 SerializedSizePosition = Ar.Tell();
		{
			int32 SerializedSize = 0;
			Ar << SerializedSize;
		}

		const int64 Start = Ar.Tell();
		if (GetScriptStruct())
		{
			check(GetStructMemory());

			if (GetScriptStruct()->IsChildOf(StaticStructFast<FVoxelVirtualStruct>()))
			{
				static_cast<FVoxelVirtualStruct*>(GetStructMemory())->PreSerialize();
			}

			GetScriptStruct()->SerializeItem(Ar, GetStructMemory(), nullptr);
		}
		const int64 End = Ar.Tell();

		Ar.Seek(SerializedSizePosition);
		{
			int32 SerializedSize = End - Start;
			Ar << SerializedSize;
		}
		Ar.Seek(End);
	}

	return true;
}

bool FVoxelInstancedStruct::Identical(const FVoxelInstancedStruct* Other, const uint32 PortFlags) const
{
	if (!ensure(Other))
	{
		return false;
	}

	if (!IsValid() &&
		!Other->IsValid())
	{
		return true;
	}

	if (GetScriptStruct() != Other->GetScriptStruct())
	{
		return false;
	}

	VOXEL_SCOPE_COUNTER_FORMAT("CompareScriptStruct %s", *GetScriptStruct()->GetName());
	return GetScriptStruct()->CompareScriptStruct(GetStructMemory(), Other->GetStructMemory(), PortFlags);
}

bool FVoxelInstancedStruct::ExportTextItem(FString& ValueStr, const FVoxelInstancedStruct& DefaultValue, UObject* Parent, const int32 PortFlags, UObject* ExportRootScope) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!IsValid())
	{
		ValueStr += "None";
		return true;
	}

	ValueStr += GetScriptStruct()->GetPathName();

	GetScriptStruct()->ExportText(
		ValueStr,
		GetStructMemory(),
		GetScriptStruct() == DefaultValue.GetScriptStruct()
		? DefaultValue.GetStructMemory()
		: nullptr,
		Parent,
		PortFlags,
		ExportRootScope);

	return true;
}

bool FVoxelInstancedStruct::ImportTextItem(const TCHAR*& Buffer, const int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive)
{
	VOXEL_FUNCTION_COUNTER();

	FString StructPathName;
	{
		const TCHAR* Result = FPropertyHelpers::ReadToken(Buffer, StructPathName, true);
		if (!Result)
		{
			return false;
		}
		Buffer = Result;
	}

	if (StructPathName.Len() == 0 ||
		StructPathName == "None")
	{
		*this = {};
		return true;
	}

	UScriptStruct* NewScriptStruct = LoadObject<UScriptStruct>(nullptr, *StructPathName);
	if (!ensure(NewScriptStruct))
	{
		return false;
	}

	if (NewScriptStruct != GetScriptStruct())
	{
		InitializeAs(NewScriptStruct);
	}

	{
		const TCHAR* Result = NewScriptStruct->ImportText(
			Buffer,
			GetStructMemory(),
			Parent,
			PortFlags,
			ErrorText,
			[&] { return NewScriptStruct->GetName(); });

		if (!Result)
		{
			return false;
		}
		Buffer = Result;
	}

	return true;
}

void FVoxelInstancedStruct::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	VOXEL_FUNCTION_COUNTER();

	if (!GetScriptStruct())
	{
		return;
	}

	TObjectPtr<UScriptStruct> ScriptStructObject = GetScriptStruct();
	Collector.AddReferencedObject(ScriptStructObject);
	check(ScriptStructObject);

	FVoxelUtilities::AddStructReferencedObjects(Collector, FVoxelStructView(*this));
}

void FVoxelInstancedStruct::GetPreloadDependencies(TArray<UObject*>& OutDependencies) const
{
	if (GetScriptStruct())
	{
		OutDependencies.Add(GetScriptStruct());
	}
}