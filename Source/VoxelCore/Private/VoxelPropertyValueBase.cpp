// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPropertyValueBase.h"
#include "Misc/DefaultValueHelper.h"

FVoxelPropertyValueBase::FVoxelPropertyValueBase(const FVoxelPropertyType& Type)
	: Type(Type)
{
	if (Type.IsStruct())
	{
		Struct = FVoxelInstancedStruct(Type.GetStruct());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelPropertyValueBase::ExportToString() const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(IsValid()))
	{
		return {};
	}

	switch (Type.GetInternalType())
	{
	default:
	{
		ensure(false);
		return {};
	}
	case EVoxelPropertyInternalType::Bool:
	{
		return LexToString(Get<bool>());
	}
	case EVoxelPropertyInternalType::Float:
	{
		return LexToString(Get<float>());
	}
	case EVoxelPropertyInternalType::Double:
	{
		return LexToString(Get<double>());
	}
	case EVoxelPropertyInternalType::Int32:
	{
		return LexToString(Get<int32>());
	}
	case EVoxelPropertyInternalType::Int64:
	{
		return LexToString(Get<int64>());
	}
	case EVoxelPropertyInternalType::Name:
	{
		return Get<FName>().ToString();
	}
	case EVoxelPropertyInternalType::Byte:
	{
		if (const UEnum* Enum = Type.GetEnum())
		{
			return Enum->GetNameStringByValue(Byte);
		}
		return LexToString(Get<uint8>());
	}
	case EVoxelPropertyInternalType::Class:
	{
		return FSoftObjectPath(GetClass()).ToString();
	}
	case EVoxelPropertyInternalType::Object:
	{
		return FSoftObjectPath(GetObject()).ToString();
	}
	case EVoxelPropertyInternalType::Struct:
	{
		if (Type.Is<FVector>())
		{
			const FVector Vector = Get<FVector>();
			return FString::Printf(TEXT("%f,%f,%f"), Vector.X, Vector.Y, Vector.Z);
		}
		else if (Type.Is<FRotator>())
		{
			const FRotator Rotator = Get<FRotator>();
			return FString::Printf(TEXT("P=%f,Y=%f,R=%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
		}
		else if (Type.Is<FQuat>())
		{
			const FRotator Rotator = Get<FQuat>().Rotator();
			return FString::Printf(TEXT("P=%f,Y=%f,R=%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
		}
		else if (Type.Is<FColor>())
		{
			const FColor& Color = Get<FColor>();
			return FString::Printf(TEXT("%d,%d,%d,%d"), Color.R, Color.G, Color.B, Color.A);
		}
		else
		{
			return FVoxelUtilities::PropertyToText_Direct(
				*FVoxelUtilities::MakeStructProperty(Type.GetStruct()),
				Struct.GetStructMemory(),
				nullptr);
		}
	}
	}
}

void FVoxelPropertyValueBase::ExportToProperty(const FProperty& Property, void* Memory) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(IsValid()) ||
		!ensure(Type.CanBeCastedTo(FVoxelPropertyType(Property))))
	{
		return;
	}

	switch (Type.GetInternalType())
	{
	default:
	{
		ensure(false);
	}
	break;
	case EVoxelPropertyInternalType::Bool:
	{
		if (!ensure(Property.IsA<FBoolProperty>()))
		{
			return;
		}

		CastFieldChecked<FBoolProperty>(Property).SetPropertyValue(Memory, Get<bool>());
	}
	break;
	case EVoxelPropertyInternalType::Float:
	{
		if (!ensure(Property.IsA<FFloatProperty>()))
		{
			return;
		}

		CastFieldChecked<FFloatProperty>(Property).SetPropertyValue(Memory, Get<float>());
	}
	break;
	case EVoxelPropertyInternalType::Double:
	{
		if (!ensure(Property.IsA<FDoubleProperty>()))
		{
			return;
		}

		CastFieldChecked<FDoubleProperty>(Property).SetPropertyValue(Memory, Get<double>());
	}
	break;
	case EVoxelPropertyInternalType::Int32:
	{
		if (!ensure(Property.IsA<FIntProperty>()))
		{
			return;
		}

		CastFieldChecked<FIntProperty>(Property).SetPropertyValue(Memory, Get<int32>());
	}
	break;
	case EVoxelPropertyInternalType::Int64:
	{
		if (!ensure(Property.IsA<FInt64Property>()))
		{
			return;
		}

		CastFieldChecked<FInt64Property>(Property).SetPropertyValue(Memory, Get<int64>());
	}
	break;
	case EVoxelPropertyInternalType::Name:
	{
		if (!ensure(Property.IsA<FNameProperty>()))
		{
			return;
		}

		CastFieldChecked<FNameProperty>(Property).SetPropertyValue(Memory, Get<FName>());
	}
	break;
	case EVoxelPropertyInternalType::Byte:
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (!ensure(Type.GetEnum() == EnumProperty->GetEnum()) ||
				!ensure(EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>()))
			{
				return;
			}

			CastFieldChecked<FByteProperty>(EnumProperty->GetUnderlyingProperty())->SetPropertyValue(Memory, Get<uint8>());
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			if (!ensure(Type.GetEnum() == ByteProperty->Enum))
			{
				return;
			}

			ByteProperty->SetPropertyValue(Memory, Get<uint8>());
		}
		else
		{
			ensure(false);
		}
	}
	break;
	case EVoxelPropertyInternalType::Class:
	{
		if (Property.IsA<FClassProperty>())
		{
			const FClassProperty& ClassProperty = CastFieldChecked<FClassProperty>(Property);
			if (!ensure(Type.GetBaseClass()->IsChildOf(ClassProperty.MetaClass)))
			{
				return;
			}

			checkUObjectAccess();
			ClassProperty.SetObjectPropertyValue(Memory, GetClass());
		}
		else if (Property.IsA<FSoftClassProperty>())
		{
			const FSoftClassProperty& ClassProperty = CastFieldChecked<FSoftClassProperty>(Property);
			if (!ensure(Type.GetBaseClass()->IsChildOf(ClassProperty.MetaClass)))
			{
				return;
			}

			checkUObjectAccess();
			ClassProperty.SetObjectPropertyValue(Memory, GetClass());
		}
		else
		{
			ensure(false);
		}
	}
	break;
	case EVoxelPropertyInternalType::Object:
	{
		if (Property.IsA<FObjectProperty>())
		{
			const FObjectProperty& ObjectProperty = CastFieldChecked<FObjectProperty>(Property);
			if (!ensure(ObjectProperty.PropertyClass == Type.GetObjectClass()))
			{
				return;
			}

			checkUObjectAccess();
			ObjectProperty.SetObjectPropertyValue(Memory, GetObject());
		}
		else if (Property.IsA<FSoftObjectProperty>())
		{
			const FSoftObjectProperty& ObjectProperty = CastFieldChecked<FSoftObjectProperty>(Property);
			if (!ensure(ObjectProperty.PropertyClass == Type.GetObjectClass()))
			{
				return;
			}

			checkUObjectAccess();
			ObjectProperty.SetObjectPropertyValue(Memory, GetObject());
		}
		else
		{
			ensure(false);
		}
	}
	break;
	case EVoxelPropertyInternalType::Struct:
	{
		if (!ensure(Property.IsA<FStructProperty>()))
		{
			return;
		}

		const FStructProperty& StructProperty = CastFieldChecked<FStructProperty>(Property);
		if (!ensure(Type.GetStruct() == StructProperty.Struct) ||
			!ensure(Type.GetStruct() == Struct.GetScriptStruct()))
		{
			return;
		}

		GetStruct().CopyTo(Memory);
	}
	}
}

bool FVoxelPropertyValueBase::ImportFromString(const FString& Value)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(IsValid()))
	{
		return false;
	}

	if (Type.GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		return ImportFromString_Array(Value);
	}

	switch (Type.GetInternalType())
	{
	default:
	{
		ensure(false);
		return false;
	}
	case EVoxelPropertyInternalType::Bool:
	{
		bBool = FCString::ToBool(*Value);
		return true;
	}
	case EVoxelPropertyInternalType::Float:
	{
		Float = FCString::Atof(*Value);
		return true;
	}
	case EVoxelPropertyInternalType::Double:
	{
		Double = FCString::Atod(*Value);
		return true;
	}
	case EVoxelPropertyInternalType::Int32:
	{
		Int32 = FCString::Atoi(*Value);
		return true;
	}
	case EVoxelPropertyInternalType::Int64:
	{
		Int64 = FCString::Atoi64(*Value);
		return true;
	}
	case EVoxelPropertyInternalType::Name:
	{
		Name = *Value;
		return true;
	}
	case EVoxelPropertyInternalType::Byte:
	{
		if (const UEnum* Enum = Type.GetEnum())
		{
			int64 EnumValue = Enum->GetValueByNameString(Value);
			if (EnumValue == -1)
			{
				EnumValue = FCString::Atoi(*Value);
			}
			if (EnumValue < 0 ||
				EnumValue > 255)
			{
				return false;
			}

			Byte = uint8(EnumValue);
			return true;
		}
		else
		{
			const int32 ByteValue = FCString::Atoi(*Value);
			if (ByteValue < 0 ||
				ByteValue > 255)
			{
				return false;
			}

			Byte = uint8(ByteValue);
			return true;
		}
	}
	case EVoxelPropertyInternalType::Class:
	{
		check(IsInGameThread());
		UClass* LoadedClass = Cast<UClass>(FSoftObjectPtr(Value).LoadSynchronous());

		if (LoadedClass &&
			!LoadedClass->IsChildOf(Type.GetBaseClass()))
		{
			return false;
		}

		Class = LoadedClass;
		return true;
	}
	case EVoxelPropertyInternalType::Object:
	{
		check(IsInGameThread());
		UObject* LoadedObject = FSoftObjectPtr(Value).LoadSynchronous();

		if (LoadedObject &&
			!LoadedObject->IsA(Type.GetObjectClass()))
		{
			return false;
		}

		Object = LoadedObject;
		return true;
	}
	case EVoxelPropertyInternalType::Struct:
	{
		Struct = FVoxelInstancedStruct(Type.GetStruct());

		if (Value.IsEmpty())
		{
			return true;
		}

#define CHECK(InType, ...) \
		if (Type.Is<InType>() && \
			Value == TEXT(#__VA_ARGS__)) \
		{ \
			Get<InType>() = __VA_ARGS__; \
			return true; \
		}

		if (Type.Is<FVector>())
		{
			CHECK(FVector, FVector::ZeroVector);
			CHECK(FVector, FVector::OneVector);

			if (Value.StartsWith(TEXT("FVector(")) &&
				Value.EndsWith(TEXT(")")))
			{
				FString Inner = Value;
				ensure(Inner.RemoveFromStart(TEXT("FVector(")));
				ensure(Inner.RemoveFromEnd(TEXT(")")));
				if (!FVoxelUtilities::IsFloat(Inner))
				{
					return false;
				}

				Get<FVector>() = FVector(FVoxelUtilities::StringToFloat(Inner));
				return true;
			}

			return FDefaultValueHelper::ParseVector(Value, Get<FVector>());
		}
		else if (Type.Is<FVector2D>())
		{
			CHECK(FVector2D, FVector2D::ZeroVector);
			CHECK(FVector2D, FVector2D::One());

			return FDefaultValueHelper::ParseVector2D(Value, Get<FVector2D>());
		}
		else if (Type.Is<FRotator>())
		{
			CHECK(FRotator, FRotator::ZeroRotator);

			return FDefaultValueHelper::ParseRotator(Value, Get<FRotator>());
		}
		else if (Type.Is<FQuat>())
		{
			CHECK(FQuat, FQuat::Identity);

			FRotator Rotator;
			if (!FDefaultValueHelper::ParseRotator(Value, Rotator))
			{
				return false;
			}

			Get<FQuat>() = Rotator.Quaternion();
			return true;
		}
		else if (Type.Is<FColor>())
		{
			CHECK(FColor, FColor::Black);
			CHECK(FColor, FColor::White);

			return FDefaultValueHelper::ParseColor(Value, Get<FColor>());
		}
		else
		{
			CHECK(FVector4, FVector4::Zero());
			CHECK(FVector4, FVector4::One());

			return FVoxelUtilities::PropertyFromText_Direct(
				*FVoxelUtilities::MakeStructProperty(Struct.GetScriptStruct()),
				Value,
				Struct.GetStructMemory(),
				nullptr);
		}

#undef CHECK
	}
	}
}

uint32 FVoxelPropertyValueBase::GetHash() const
{
	if (!IsValid())
	{
		return 0;
	}

	if (Type.GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		return GetHash_Array();
	}

	switch (Type.GetInternalType())
	{
	default:
	{
		ensure(false);
		return 0;
	}
	case EVoxelPropertyInternalType::Bool:
	{
		return GetTypeHash(Get<bool>());
	}
	case EVoxelPropertyInternalType::Float:
	{
		return GetTypeHash(Get<float>());
	}
	case EVoxelPropertyInternalType::Double:
	{
		return GetTypeHash(Get<double>());
	}
	case EVoxelPropertyInternalType::Int32:
	{
		return GetTypeHash(Get<int32>());
	}
	case EVoxelPropertyInternalType::Int64:
	{
		return GetTypeHash(Get<int64>());
	}
	case EVoxelPropertyInternalType::Name:
	{
		return GetTypeHash(Get<FName>());
	}
	case EVoxelPropertyInternalType::Byte:
	{
		return GetTypeHash(Get<uint8>());
	}
	case EVoxelPropertyInternalType::Class:
	{
		return GetTypeHash(GetClass());
	}
	case EVoxelPropertyInternalType::Object:
	{
		return GetTypeHash(GetObject());
	}
	case EVoxelPropertyInternalType::Struct:
	{
		if (!Struct.IsValid())
		{
			return 0;
		}

		return Struct.GetScriptStruct()->GetStructTypeHash(Struct.GetStructMemory());
	}
	}
}

void FVoxelPropertyValueBase::Fixup()
{
	if (Is<FBodyInstance>())
	{
		Get<FBodyInstance>().LoadProfileData(false);
	}

	if (Is<uint8>())
	{
		if (const UEnum* Enum = Type.GetEnum())
		{
			if (!Enum->IsValidEnumValue(Byte))
			{
				Byte = Enum->GetValueByIndex(0);
			}
			else if (
				Enum->NumEnums() > 1 &&
				Byte == Enum->GetMaxEnumValue())
			{
				Byte = Enum->GetValueByIndex(0);
			}
		}
	}

	if (Type.GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		Fixup_Array();
	}
}

bool FVoxelPropertyValueBase::IsValid() const
{
	if (!Type.IsValid())
	{
		return false;
	}

	if (!HasArray() &&
		!ensure(Type.GetContainerType() != EVoxelPropertyContainerType::Array))
	{
		return false;
	}

	if (Type.IsStruct())
	{
		return ensure(Type.GetStruct() == Struct.GetScriptStruct());
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelPropertyValueBase::Serialize(FArchive& Ar)
{
	INLINE_LAMBDA
	{
		if (!Ar.IsSaving())
		{
			return;
		}

		EnumValueName = {};

		if (!Type.Is<uint8>())
		{
			return;
		}

		const UEnum* Enum = Type.GetEnum();
		if (!Enum ||
			!ensure(Enum->IsValidEnumValue(Byte)))
		{
			return;
		}

		EnumValueName = Enum->GetNameByValue(Byte);
	};

	// Returning false to fall back to default serialization
	return false;
}

void FVoxelPropertyValueBase::PostSerialize(const FArchive& Ar)
{
	if (!Ar.IsLoading() ||
		EnumValueName.IsNone() ||
		!ensure(Type.Is<uint8>()))
	{
		return;
	}

	const UEnum* Enum = Type.GetEnum();
	if (!Enum)
	{
		return;
	}

	const int64 EnumValue = Enum->GetValueByName(EnumValueName);
	if (!ensure(FVoxelUtilities::IsValidUINT8(EnumValue)))
	{
		return;
	}

	Byte = EnumValue;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPropertyValueBase FVoxelPropertyValueBase::MakeStruct(const FConstVoxelStructView Struct)
{
	check(Struct.IsValid());

	FVoxelPropertyValueBase Result;
	Result.Type = FVoxelPropertyType::MakeStruct(Struct.GetStruct());
	Result.Struct = Struct.MakeInstancedStruct();
	return Result;
}

FVoxelPropertyValueBase FVoxelPropertyValueBase::MakeFromProperty(const FProperty& Property, const void* Memory)
{
	FVoxelPropertyValueBase Result = FVoxelPropertyValueBase(FVoxelPropertyType(Property));
	if (!ensure(Result.Type.GetContainerType() != EVoxelPropertyContainerType::Array))
	{
		return {};
	}

	if (Property.IsA<FBoolProperty>())
	{
		Result.Get<bool>() = CastFieldChecked<FBoolProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FFloatProperty>())
	{
		Result.Get<float>() = CastFieldChecked<FFloatProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FDoubleProperty>())
	{
		Result.Get<double>() = CastFieldChecked<FDoubleProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FIntProperty>())
	{
		Result.Get<int32>() = CastFieldChecked<FIntProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FInt64Property>())
	{
		Result.Get<int64>() = CastFieldChecked<FInt64Property>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FNameProperty>())
	{
		Result.Get<FName>() = CastFieldChecked<FNameProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FByteProperty>())
	{
		Result.Get<uint8>() = CastFieldChecked<FByteProperty>(Property).GetPropertyValue(Memory);
	}
	else if (Property.IsA<FEnumProperty>())
	{
		FNumericProperty* UnderlyingProperty = CastFieldChecked<FEnumProperty>(Property).GetUnderlyingProperty();
		if (!ensure(UnderlyingProperty->IsA<FByteProperty>()))
		{
			return {};
		}
		Result.Get<uint8>() = CastFieldChecked<FByteProperty>(UnderlyingProperty)->GetPropertyValue(Memory);
	}
	else if (Property.IsA<FClassProperty>())
	{
		const FClassProperty& ClassProperty = CastFieldChecked<FClassProperty>(Property);
		ensure(ClassProperty.MetaClass == Result.Type.GetBaseClass());

		Result.GetClass() = Cast<UClass>(ClassProperty.GetObjectPropertyValue(Memory));
	}
	else if (Property.IsA<FSoftClassProperty>())
	{
		const FSoftClassProperty& ClassProperty = CastFieldChecked<FSoftClassProperty>(Property);
		ensure(ClassProperty.MetaClass == Result.Type.GetBaseClass());

		Result.GetClass() = Cast<UClass>(ClassProperty.GetObjectPropertyValue(Memory));
	}
	else if (Property.IsA<FObjectProperty>())
	{
		const FObjectProperty& ObjectProperty = CastFieldChecked<FObjectProperty>(Property);
		ensure(ObjectProperty.PropertyClass == Result.Type.GetObjectClass());

		Result.GetObject() = ObjectProperty.GetObjectPropertyValue(Memory);
	}
	else if (Property.IsA<FSoftObjectProperty>())
	{
		const FSoftObjectProperty& ObjectProperty = CastFieldChecked<FSoftObjectProperty>(Property);
		ensure(ObjectProperty.PropertyClass == Result.Type.GetObjectClass());

		Result.GetObject() = ObjectProperty.GetObjectPropertyValue(Memory);
	}
	else if (Property.IsA<FStructProperty>())
	{
		const UScriptStruct* Struct = CastFieldChecked<FStructProperty>(Property).Struct;
		ensure(Result.GetStruct().GetScriptStruct() == Struct);
		Result.GetStruct().CopyFrom(Memory);
	}
	else
	{
		ensure(false);
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelPropertyValueBase::operator==(const FVoxelPropertyValueBase& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}

	if (!Type.IsValid())
	{
		return true;
	}
	if (!ensure(IsValid()) ||
		!ensure(Other.IsValid()))
	{
		return false;
	}

	if (Type.GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		return Equal_Array(Other);
	}

	switch (Type.GetInternalType())
	{
	default:
	{
		ensure(false);
		return false;
	}
	case EVoxelPropertyInternalType::Bool:
	{
		return Get<bool>() == Other.Get<bool>();
	}
	case EVoxelPropertyInternalType::Float:
	{
		return Get<float>() == Other.Get<float>();
	}
	case EVoxelPropertyInternalType::Double:
	{
		return Get<double>() == Other.Get<double>();
	}
	case EVoxelPropertyInternalType::Int32:
	{
		return Get<int32>() == Other.Get<int32>();
	}
	case EVoxelPropertyInternalType::Int64:
	{
		return Get<int64>() == Other.Get<int64>();
	}
	case EVoxelPropertyInternalType::Name:
	{
		return Get<FName>() == Other.Get<FName>();
	}
	case EVoxelPropertyInternalType::Byte:
	{
		return Get<uint8>() == Other.Get<uint8>();
	}
	case EVoxelPropertyInternalType::Class:
	{
		checkUObjectAccess();
		return GetClass() == Other.GetClass();
	}
	case EVoxelPropertyInternalType::Object:
	{
		checkUObjectAccess();
		return GetObject() == Other.GetObject();
	}
	case EVoxelPropertyInternalType::Struct:
	{
		return GetStruct() == Other.GetStruct();
	}
	}
}