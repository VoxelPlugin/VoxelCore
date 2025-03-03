// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPropertyType.h"
#include "VoxelPropertyValue.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/CoreRedirects.h"
#if WITH_EDITOR
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Textures/SlateIcon.h"
#include "GraphEditorSettings.h"
#endif

FVoxelPropertyType::FVoxelPropertyType(const FProperty& Property)
{
	if (Property.IsA<FArrayProperty>())
	{
		*this = FVoxelPropertyType(*CastFieldChecked<FArrayProperty>(Property).Inner);
		ContainerType = EVoxelPropertyContainerType::Array;
	}
	else if (Property.IsA<FBoolProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Bool;
		ensure(CastFieldChecked<FBoolProperty>(Property).IsNativeBool());
	}
	else if (Property.IsA<FFloatProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Float;
	}
	else if (Property.IsA<FDoubleProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Double;
	}
	else if (Property.IsA<FIntProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Int32;
	}
	else if (Property.IsA<FInt64Property>())
	{
		InternalType = EVoxelPropertyInternalType::Int64;
	}
	else if (Property.IsA<FNameProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Name;
	}
	else if (Property.IsA<FByteProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Byte;
		PrivateInternalField = CastFieldChecked<FByteProperty>(Property).Enum;
	}
	else if (Property.IsA<FEnumProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Byte;
		PrivateInternalField = CastFieldChecked<FEnumProperty>(Property).GetEnum();
	}
	else if (Property.IsA<FClassProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Class;
		PrivateInternalField = CastFieldChecked<FClassProperty>(Property).MetaClass;
	}
	else if (Property.IsA<FSoftClassProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Class;
		PrivateInternalField = CastFieldChecked<FSoftClassProperty>(Property).MetaClass;
	}
	else if (Property.IsA<FObjectProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Object;
		PrivateInternalField = CastFieldChecked<FObjectProperty>(Property).PropertyClass;
	}
	else if (Property.IsA<FSoftObjectProperty>())
	{
		InternalType = EVoxelPropertyInternalType::Object;
		PrivateInternalField = CastFieldChecked<FSoftObjectProperty>(Property).PropertyClass;
	}
	else if (Property.IsA<FStructProperty>())
	{
		*this = MakeStruct(CastFieldChecked<FStructProperty>(Property).Struct);
	}
	else
	{
		ensure(false);
	}

	ensure(IsValid());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPropertyType FVoxelPropertyType::MakeStruct(UScriptStruct* Struct)
{
	checkVoxelSlow(Struct);
	checkVoxelSlow(Struct != StaticStructFast<FVoxelPropertyType>());
	checkVoxelSlow(Struct != StaticStructFast<FVoxelPropertyValue>());

	return MakeImpl(EVoxelPropertyInternalType::Struct, Struct);
}

FVoxelPropertyType FVoxelPropertyType::MakeFromK2(const FEdGraphPinType& PinType)
{
	// From UEdGraphSchema_K2

	FVoxelPropertyType Type = INLINE_LAMBDA -> FVoxelPropertyType
	{
		if (PinType.PinCategory == STATIC_FNAME("bool"))
		{
			return Make<bool>();
		}
		else if (PinType.PinCategory == STATIC_FNAME("real"))
		{
			if (PinType.PinSubCategory == STATIC_FNAME("float"))
			{
				return Make<float>();
			}
			else
			{
				ensure(PinType.PinSubCategory == STATIC_FNAME("double"));
				return Make<double>();
			}
		}
		else if (PinType.PinCategory == STATIC_FNAME("int"))
		{
			return Make<int32>();
		}
		else if (PinType.PinCategory == STATIC_FNAME("int64"))
		{
			return Make<int64>();
		}
		else if (PinType.PinCategory == STATIC_FNAME("name"))
		{
			return Make<FName>();
		}
		else if (PinType.PinCategory == STATIC_FNAME("byte"))
		{
			if (UEnum* EnumType = Cast<UEnum>(PinType.PinSubCategoryObject.Get()))
			{
				return MakeEnum(EnumType);
			}
			else
			{
				return Make<uint8>();
			}
		}
		else if (PinType.PinCategory == STATIC_FNAME("class"))
		{
			if (UClass* ClassType = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
			{
				return MakeClass(ClassType);
			}
			else
			{
				return {};
			}
		}
		else if (
			PinType.PinCategory == STATIC_FNAME("object") ||
			PinType.PinCategory == STATIC_FNAME("interface"))
		{
			if (UClass* ObjectType = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
			{
				return MakeObject(ObjectType);
			}
			else
			{
				return {};
			}
		}
		else if (PinType.PinCategory == STATIC_FNAME("struct"))
		{
			if (UScriptStruct* StructType = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
			{
				return MakeStruct(StructType);
			}
			else
			{
				return {};
			}
		}
		else
		{
			return {};
		}
	};

	Type.ContainerType = PinType.IsArray() ? EVoxelPropertyContainerType::Array : EVoxelPropertyContainerType::None;
	return Type;
}

bool FVoxelPropertyType::TryParse(const FString& TypeString, FVoxelPropertyType& OutType)
{
	VOXEL_SCOPE_COUNTER("FindObject");

	// Serializing structs directly doesn't seem to handle redirects properly
	const FCoreRedirectObjectName RedirectedName = FCoreRedirects::GetRedirectedName(
		ECoreRedirectFlags::Type_Struct,
		FCoreRedirectObjectName(TypeString));

	UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *RedirectedName.ToString());
	if (!ensure(Struct))
	{
		return false;
	}

	OutType = MakeStruct(Struct);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelPropertyType::IsValid() const
{
	switch (InternalType)
	{
	case EVoxelPropertyInternalType::Invalid:
	{
		ensure(!GetInternalField());
		return false;
	}
	case EVoxelPropertyInternalType::Bool:
	case EVoxelPropertyInternalType::Float:
	case EVoxelPropertyInternalType::Double:
	case EVoxelPropertyInternalType::Int32:
	case EVoxelPropertyInternalType::Int64:
	case EVoxelPropertyInternalType::Name:
	{
		return ensure(!GetInternalField());
	}
	case EVoxelPropertyInternalType::Byte:
	{
		if (!GetInternalField())
		{
			return true;
		}

		return ensureVoxelSlow(Cast<UEnum>(GetInternalField()));
	}
	case EVoxelPropertyInternalType::Class:
	{
		return ensureVoxelSlow(Cast<UClass>(GetInternalField()));
	}
	case EVoxelPropertyInternalType::Object:
	{
		return ensureVoxelSlow(Cast<UClass>(GetInternalField()));
	}
	case EVoxelPropertyInternalType::Struct:
	{
		return ensureVoxelSlow(Cast<UScriptStruct>(GetInternalField()));
	}
	default:
	{
		ensure(false);
		return false;
	}
	}
}

FString FVoxelPropertyType::ToString() const
{
	if (!ensureVoxelSlow(IsValid()))
	{
		return "INVALID";
	}

	FString Name = INLINE_LAMBDA -> FString
	{
		switch (InternalType)
		{
		default: ensure(false);
		case EVoxelPropertyInternalType::Bool: return "Boolean";
		case EVoxelPropertyInternalType::Float: return "Float";
		case EVoxelPropertyInternalType::Double: return "Double";
		case EVoxelPropertyInternalType::Int32: return "Integer";
		case EVoxelPropertyInternalType::Int64: return "Integer 64";
		case EVoxelPropertyInternalType::Name: return "Name";
		case EVoxelPropertyInternalType::Byte:
		{
			if (const UEnum* Enum = GetEnum())
			{
	#if WITH_EDITOR
				return Enum->GetDisplayNameText().ToString();
	#else
				return Enum->GetName();
	#endif
			}
			else
			{
				return "Byte";
			}
		}
		case EVoxelPropertyInternalType::Class:
		{
	#if WITH_EDITOR
			return GetBaseClass()->GetDisplayNameText().ToString() + " Class";
	#else
			return GetBaseClass()->GetName() + " Class";
	#endif
		}
		case EVoxelPropertyInternalType::Object:
		{
	#if WITH_EDITOR
			return GetObjectClass()->GetDisplayNameText().ToString();
	#else
			return GetObjectClass()->GetName();
	#endif
		}
		case EVoxelPropertyInternalType::Struct:
		{
	#if WITH_EDITOR
			return GetStruct()->GetDisplayNameText().ToString();
	#else
			return GetStruct()->GetName();
	#endif
		}
		}
	};

	Name.RemoveFromStart("Voxel ");
	Name.RemoveFromStart("EVoxel ");
	return Name;
}

int32 FVoxelPropertyType::GetTypeSize() const
{
	check(GetContainerType() == EVoxelPropertyContainerType::None);

	if (!ensure(IsValid()))
	{
		return 0;
	}

	switch (InternalType)
	{
	default:
	{
		ensure(false);
		return 0;
	}
	case EVoxelPropertyInternalType::Bool: return sizeof(bool);
	case EVoxelPropertyInternalType::Float: return sizeof(float);
	case EVoxelPropertyInternalType::Double: return sizeof(double);
	case EVoxelPropertyInternalType::Int32: return sizeof(int32);
	case EVoxelPropertyInternalType::Int64: return sizeof(int64);
	case EVoxelPropertyInternalType::Name: return sizeof(FName);
	case EVoxelPropertyInternalType::Byte: return sizeof(uint8);
	case EVoxelPropertyInternalType::Class: return sizeof(UClass*);
	case EVoxelPropertyInternalType::Object: return sizeof(UObject*);
	case EVoxelPropertyInternalType::Struct: return GetStruct()->GetStructureSize();
	}
}

FEdGraphPinType FVoxelPropertyType::GetEdGraphPinType_K2() const
{
	if (!IsValid())
	{
		return {};
	}

	// From UEdGraphSchema_K2

	FEdGraphPinType PinType;
	PinType.ContainerType = ContainerType == EVoxelPropertyContainerType::Array ? EPinContainerType::Array : EPinContainerType::None;

	switch (InternalType)
	{
	case EVoxelPropertyInternalType::Bool:
	{
		PinType.PinCategory = STATIC_FNAME("bool");
		return PinType;
	}
	// Always use double with blueprints
	case EVoxelPropertyInternalType::Float:
	case EVoxelPropertyInternalType::Double:
	{
		PinType.PinCategory = STATIC_FNAME("real");
		PinType.PinSubCategory = STATIC_FNAME("double");
		return PinType;
	}
	case EVoxelPropertyInternalType::Int32:
	{
		PinType.PinCategory = STATIC_FNAME("int");
		return PinType;
	}
	case EVoxelPropertyInternalType::Int64:
	{
		PinType.PinCategory = STATIC_FNAME("int64");
		return PinType;
	}
	case EVoxelPropertyInternalType::Name:
	{
		PinType.PinCategory = STATIC_FNAME("name");
		return PinType;
	}
	case EVoxelPropertyInternalType::Byte:
	{
		PinType.PinCategory = STATIC_FNAME("byte");
		PinType.PinSubCategoryObject = GetEnum();
		return PinType;
	}
	case EVoxelPropertyInternalType::Class:
	{
		PinType.PinCategory = STATIC_FNAME("class");
		PinType.PinSubCategoryObject = GetBaseClass();
		return PinType;
	}
	case EVoxelPropertyInternalType::Object:
	{
		PinType.PinCategory = STATIC_FNAME("object");
		PinType.PinSubCategoryObject = GetObjectClass();
		return PinType;
	}
	case EVoxelPropertyInternalType::Struct:
	{
		PinType.PinCategory = STATIC_FNAME("struct");
		PinType.PinSubCategoryObject = GetStruct();
		return PinType;
	}
	default:
	{
		ensure(false);
		PinType.PinCategory = STATIC_FNAME("wildcard");
		return PinType;
	}
	}
}

bool FVoxelPropertyType::CanBeCastedTo(const FVoxelPropertyType& Other) const
{
	if (*this == Other)
	{
		return true;
	}

	if (InternalType != Other.InternalType ||
		ContainerType != Other.ContainerType)
	{
		return false;
	}

	switch (InternalType)
	{
	default:
	{
		return
			ensureVoxelSlow(!GetInternalField()) &&
			ensureVoxelSlow(!Other.GetInternalField());
	}
	case EVoxelPropertyInternalType::Byte:
	{
		// Enums can be casted to byte and the other way around
		return true;
	}
	case EVoxelPropertyInternalType::Class:
	{
		// Classes can always be casted, check is done in TSubclassOf::Get
		return true;
	}
	case EVoxelPropertyInternalType::Object:
	{
		const UClass* Class = Cast<UClass>(GetInternalField());
		const UClass* OtherClass = Cast<UClass>(Other.GetInternalField());

		if (!ensureVoxelSlow(Class) ||
			!ensureVoxelSlow(OtherClass))
		{
			return false;
		}

		return Class->IsChildOf(OtherClass);
	}
	case EVoxelPropertyInternalType::Struct:
	{
		const UScriptStruct* Struct = Cast<UScriptStruct>(GetInternalField());
		const UScriptStruct* OtherStruct = Cast<UScriptStruct>(Other.GetInternalField());

		if (!ensureVoxelSlow(Struct) ||
			!ensureVoxelSlow(OtherStruct))
		{
			return false;
		}

		return Struct->IsChildOf(OtherStruct);
	}
	}
}

#if WITH_EDITOR
FSlateIcon FVoxelPropertyType::GetIcon() const
{
	static const FSlateIcon VariableIcon(FAppStyle::GetAppStyleSetName(), "Kismet.VariableList.TypeIcon");

	if (GetContainerType() == EVoxelPropertyContainerType::Array)
	{
		static const FSlateIcon ArrayIcon(FAppStyle::GetAppStyleSetName(), "Kismet.VariableList.ArrayTypeIcon");
		return ArrayIcon;
	}

	if (IsClass())
	{
		if (const UClass* Class = GetBaseClass())
		{
			return FSlateIconFinder::FindIconForClass(Class);
		}
		else
		{
			return VariableIcon;
		}
	}

	if (IsObject())
	{
		if (const UClass* Class = GetObjectClass())
		{
			return FSlateIconFinder::FindIconForClass(Class);
		}
		else
		{
			return VariableIcon;
		}
	}

	return VariableIcon;
}

FLinearColor FVoxelPropertyType::GetColor() const
{
	// Containers have the same color as their inner type
	const FVoxelPropertyType Type = GetInnerType();

	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();

	if (Type.Is<bool>())
	{
		return Settings->BooleanPinTypeColor;
	}
	else if (Type.Is<float>())
	{
		return Settings->FloatPinTypeColor;
	}
	else if (Type.Is<double>())
	{
		return Settings->DoublePinTypeColor;
	}
	else if (Type.Is<int32>())
	{
		return Settings->IntPinTypeColor;
	}
	else if (Type.Is<int64>())
	{
		return Settings->Int64PinTypeColor;
	}
	else if (Type.Is<FName>())
	{
		return Settings->StringPinTypeColor;
	}
	else if (Type.Is<uint8>())
	{
		return Settings->BytePinTypeColor;
	}
	else if (Type.Is<FVector>())
	{
		return Settings->VectorPinTypeColor;
	}
	else if (
		Type.Is<FRotator>() ||
		Type.Is<FQuat>())
	{
		return Settings->RotatorPinTypeColor;
	}
	else if (Type.Is<FTransform>())
	{
		return Settings->TransformPinTypeColor;
	}
	else if (Type.Is<FVoxelFloatRange>())
	{
		return Settings->FloatPinTypeColor;
	}
	else if (Type.Is<FVoxelInt32Range>())
	{
		return Settings->IntPinTypeColor;
	}
	else if (Type.IsClass())
	{
		return Settings->ClassPinTypeColor;
	}
	else if (Type.IsObject())
	{
		return Settings->ObjectPinTypeColor;
	}
	else if (Type.IsStruct())
	{
		return Settings->StructPinTypeColor;
	}
	else
	{
		ensureVoxelSlow(false);
		return Settings->DefaultPinTypeColor;
	}
}
#endif