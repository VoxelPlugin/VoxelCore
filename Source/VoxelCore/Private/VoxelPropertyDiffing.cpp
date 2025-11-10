// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPropertyDiffing.h"

void FVoxelPropertyDiffing::Traverse(
	const FProperty& Property,
	const FString& BasePath,
	const void* OldMemory,
	const void* NewMemory,
	TVoxelArray<FString>& OutChanges)
{
	if (Property.Identical(OldMemory, NewMemory))
	{
		return;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		const UScriptStruct& Struct = *StructProperty->Struct;

		if (!(Struct.StructFlags & STRUCT_IdenticalNative))
		{
			for (const FProperty& InnerProperty : GetStructProperties(StructProperty->Struct))
			{
				for (int32 Index = 0; Index < InnerProperty.ArrayDim; Index++)
				{
					Traverse(
						InnerProperty,
						BasePath + "." + InnerProperty.GetName() + (InnerProperty.ArrayDim == 1 ? FString() : FString::Printf(TEXT("[%d]"), Index)),
						InnerProperty.ContainerPtrToValuePtr<void>(OldMemory, Index),
						InnerProperty.ContainerPtrToValuePtr<void>(NewMemory, Index),
						OutChanges);
				}
			}

			return;
		}
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper OldArray(ArrayProperty, OldMemory);
		FScriptArrayHelper NewArray(ArrayProperty, NewMemory);

		const int32 OldNum = OldArray.Num();
		const int32 NewNum = NewArray.Num();
		const int32 MinNum = FMath::Min(OldNum, NewNum);

		// Compare common elements
		for (int32 Index = 0; Index < MinNum; Index++)
		{
			Traverse(
				*ArrayProperty->Inner,
				BasePath + FString::Printf(TEXT("[%d]"), Index),
				OldArray.GetRawPtr(Index),
				NewArray.GetRawPtr(Index),
				OutChanges);
		}

		// Report removed elements
		for (int32 Index = MinNum; Index < OldNum; Index++)
		{
			OutChanges.Add(
				BasePath + FString::Printf(TEXT("[%d]"), Index) + ": " +
				ToString(*ArrayProperty->Inner, OldArray.GetRawPtr(Index)) +
				" removed");
		}

		// Report added elements
		for (int32 Index = MinNum; Index < NewNum; Index++)
		{
			OutChanges.Add(
				BasePath + FString::Printf(TEXT("[%d]"), Index) + ": " +
				ToString(*ArrayProperty->Inner, NewArray.GetRawPtr(Index)) +
				" added");
		}

		return;
	}

	if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper OldMap(MapProperty, OldMemory);
		FScriptMapHelper NewMap(MapProperty, NewMemory);

		// Check for removed or changed keys
		for (FScriptMapHelper::FIterator It = OldMap.CreateIterator(); It; ++It)
		{
			const void* OldKeyPtr = OldMap.GetKeyPtr(It);
			const void* OldValuePtr = OldMap.GetValuePtr(It);
			const void* NewValuePtr = NewMap.FindValueFromHash(OldKeyPtr);

			if (!NewValuePtr)
			{
				OutChanges.Add(
					BasePath + ": Key " +
					ToString(*MapProperty->KeyProp, OldKeyPtr) +
					" removed");

				continue;
			}

			Traverse(
				*MapProperty->ValueProp,
				BasePath + "[" + ToString(*MapProperty->KeyProp, OldKeyPtr) + "]",
				OldValuePtr,
				NewValuePtr,
				OutChanges);
		}

		// Check for added keys
		for (FScriptMapHelper::FIterator It = NewMap.CreateIterator(); It; ++It)
		{
			const void* NewKeyPtr = NewMap.GetKeyPtr(It);
			const void* OldValuePtr = OldMap.FindValueFromHash(NewKeyPtr);

			if (!OldValuePtr)
			{
				OutChanges.Add(
					BasePath + ": Key " +
					ToString(*MapProperty->KeyProp, NewKeyPtr) +
					" added");
			}
		}

		return;
	}

	OutChanges.Add(
		BasePath + ": " +
		ToString(Property, OldMemory) + " -> " +
		ToString(Property, NewMemory));
}

FString FVoxelPropertyDiffing::ToString(
	const FProperty& Property,
	const void* Memory)
{
	if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		return MakeVoxelObjectPtr(ObjectProperty->GetObjectPropertyValue(Memory)).GetPathName();
	}

	return FVoxelUtilities::PropertyToText_Direct(Property, Memory, nullptr);
}