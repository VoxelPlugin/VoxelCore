// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelPropertyValue.h"
#include "EdGraph/EdGraphPin.h"

FVoxelPropertyTerminalValue::FVoxelPropertyTerminalValue(const FVoxelPropertyType& Type)
	: FVoxelPropertyValueBase(Type)
{
	ensure(Type.GetContainerType() == EVoxelPropertyContainerType::None);
	Fixup();
}

FVoxelPropertyValue FVoxelPropertyTerminalValue::ToValue() const
{
	return FVoxelPropertyValue(FVoxelPropertyValueBase(*this));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPropertyValue::FVoxelPropertyValue(const FVoxelPropertyType& Type)
	: FVoxelPropertyValueBase(Type)
{
	Fixup();
}

FVoxelPropertyValue FVoxelPropertyValue::MakeFromK2PinDefaultValue(const UEdGraphPin& Pin)
{
	const FVoxelPropertyType Type = FVoxelPropertyType::MakeFromK2(Pin.PinType);
	if (!ensure(Type.IsValid()))
	{
		return {};
	}

	FVoxelPropertyValue Result(Type);

	if (Pin.DefaultObject)
	{
		ensure(Pin.DefaultValue.IsEmpty());

		if (Type.IsClass())
		{
			Result.GetClass() = Cast<UClass>(Pin.DefaultObject);
		}
		else if (Type.IsObject())
		{
			Result.GetObject() = Pin.DefaultObject;
		}
		else
		{
			ensure(false);
		}
	}
	else if (!Pin.DefaultValue.IsEmpty())
	{
		ensure(!Type.IsObject());
		ensure(Result.ImportFromString(Pin.DefaultValue));
	}

	return Result;
}

FVoxelPropertyValue FVoxelPropertyValue::MakeStruct(const FConstVoxelStructView Struct)
{
	return FVoxelPropertyValue(Super::MakeStruct(Struct));
}

FVoxelPropertyValue FVoxelPropertyValue::MakeFromProperty(const FProperty& Property, const void* Memory)
{
	const FVoxelPropertyType Type(Property);
	if (Type.GetContainerType() == EVoxelPropertyContainerType::None)
	{
		return FVoxelPropertyValue(Super::MakeFromProperty(Property, Memory));
	}

	const FArrayProperty& ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
	FScriptArrayHelper ArrayHelper(&ArrayProperty, Memory);

	FVoxelPropertyValue Result(Type);
	for (int32 Index = 0; Index < ArrayHelper.Num(); Index++)
	{
		Result.Array.Add(FVoxelPropertyTerminalValue::MakeFromProperty(
			*ArrayProperty.Inner,
			ArrayHelper.GetRawPtr(Index)));
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelPropertyValue::Fixup(const FVoxelPropertyType& NewType)
{
	if (!IsValid() ||
		!CanBeCastedTo(NewType))
	{
		*this = FVoxelPropertyValue(NewType);
	}

	Fixup();
}

bool FVoxelPropertyValue::ImportFromUnrelated(FVoxelPropertyValue Other)
{
	VOXEL_FUNCTION_COUNTER();

	if (GetType() == Other.GetType())
	{
		*this = Other;
		return true;
	}

	if (Other.Is<FColor>())
	{
		Other = Make(FLinearColor(
			Other.Get<FColor>().R,
			Other.Get<FColor>().G,
			Other.Get<FColor>().B,
			Other.Get<FColor>().A));
	}
	if (Other.Is<FQuat>())
	{
		Other = Make(Other.Get<FQuat>().Rotator());
	}
	if (Other.Is<FRotator>())
	{
		Other = Make(FVector(
			Other.Get<FRotator>().Pitch,
			Other.Get<FRotator>().Yaw,
			Other.Get<FRotator>().Roll));
	}
	if (Other.Is<int32>())
	{
		Other = Make<float>(Other.Get<int32>());
	}
	if (Other.Is<FIntPoint>())
	{
		Other = Make(FVector2D(
			Other.Get<FIntPoint>().X,
			Other.Get<FIntPoint>().Y));
	}
	if (Other.Is<FIntVector>())
	{
		Other = Make(FVector(
			Other.Get<FIntVector>().X,
			Other.Get<FIntVector>().Y,
			Other.Get<FIntVector>().Z));
	}

#define CHECK(NewType, OldType, ...) \
	if (Is<NewType>() && Other.Is<OldType>()) \
	{ \
		const OldType Value = Other.Get<OldType>(); \
		Get<NewType>() = __VA_ARGS__; \
		return true; \
	}

	CHECK(float, FVector2D, Value.X);
	CHECK(float, FVector, Value.X);
	CHECK(float, FLinearColor, Value.R);

	CHECK(FVector2D, float, FVector2D(Value));
	CHECK(FVector2D, FVector, FVector2D(Value));
	CHECK(FVector2D, FLinearColor, FVector2D(Value));

	CHECK(FVector, float, FVector(Value));
	CHECK(FVector, FVector2D, FVector(Value, 0.f));
	CHECK(FVector, FLinearColor, FVector(Value));

	CHECK(FLinearColor, float, FLinearColor(Value, Value, Value, Value));
	CHECK(FLinearColor, FVector2D, FLinearColor(Value.X, Value.Y, 0.f));
	CHECK(FLinearColor, FVector, FLinearColor(Value));

	CHECK(int32, float, Value);
	CHECK(int32, FVector2D, Value.X);
	CHECK(int32, FVector, Value.X);
	CHECK(int32, FLinearColor, Value.R);

	CHECK(FIntPoint, float, FIntPoint(Value));
	CHECK(FIntPoint, FVector2D, FIntPoint(Value.X, Value.Y));
	CHECK(FIntPoint, FVector, FIntPoint(Value.X, Value.Y));
	CHECK(FIntPoint, FLinearColor, FIntPoint(Value.R, Value.G));

	CHECK(FIntVector, float, FIntVector(Value));
	CHECK(FIntVector, FVector2D, FIntVector(Value.X, Value.Y, 0));
	CHECK(FIntVector, FVector, FIntVector(Value.X, Value.Y, Value.Z));
	CHECK(FIntVector, FLinearColor, FIntVector(Value.R, Value.G, Value.B));

#undef CHECK

	return ImportFromString(Other.ExportToString());
}

FVoxelPropertyTerminalValue FVoxelPropertyValue::ToTerminalValue() const
{
	if (!ensure(!IsArray()))
	{
		return {};
	}

	return FVoxelPropertyTerminalValue(FVoxelPropertyValueBase(*this));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelPropertyValue::ExportToString_Array() const
{
	check(IsArray());
	ensure(false);
	return {};
}

void FVoxelPropertyValue::ExportToProperty_Array(const FProperty& Property, void* Memory) const
{
	check(IsArray());

	if (!ensure(Property.IsA<FArrayProperty>()))
	{
		return;
	}

	const FArrayProperty& ArrayProperty = CastFieldChecked<FArrayProperty>(Property);

	FScriptArrayHelper ArrayHelper(&ArrayProperty, Memory);
	ArrayHelper.Resize(Array.Num());
	for (int32 Index = 0; Index < Array.Num(); Index++)
	{
		Array[Index].ExportToProperty(*ArrayProperty.Inner, ArrayHelper.GetRawPtr(Index));
	}
}

bool FVoxelPropertyValue::ImportFromString_Array(const FString& Value)
{
	check(IsArray());
	ensure(false);
	return false;
}

uint32 FVoxelPropertyValue::GetHash_Array() const
{
	check(IsArray());

	if (Array.Num() == 0)
	{
		return 0;
	}

	return
		GetTypeHash(Array.Num()) ^
		GetTypeHash(Array[0]);
}

void FVoxelPropertyValue::Fixup_Array()
{
	for (FVoxelPropertyTerminalValue& Value : Array)
	{
		if (!Value.IsValid() ||
			!Value.CanBeCastedTo(Type.GetInnerType()))
		{
			Value = FVoxelPropertyTerminalValue(Type.GetInnerType());
		}
		Value.Fixup();
	}
}

bool FVoxelPropertyValue::Equal_Array(const FVoxelPropertyValueBase& Other) const
{
	check(IsArray());
	const FVoxelPropertyValue& OtherValue = static_cast<const FVoxelPropertyValue&>(Other);

	check(OtherValue.IsArray());
	return Array == OtherValue.Array;
}