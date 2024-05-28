// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPropertyType.generated.h"

struct FSlateIcon;
struct FEdGraphPinType;
struct FVoxelPropertyType;
struct FVoxelPropertyValue;

template<typename>
struct TIsSafeVoxelPropertyType
{
	static constexpr bool Value = true;
};

template<>
struct TIsSafeVoxelPropertyType<FVoxelPropertyType>
{
	static constexpr bool Value = false;
};
template<>
struct TIsSafeVoxelPropertyType<FVoxelPropertyValue>
{
	static constexpr bool Value = false;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TIsSafeVoxelPropertyValue
{
	static constexpr bool Value = TIsSafeVoxelPropertyType<T>::Value;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UENUM()
enum class EVoxelPropertyInternalType : uint8
{
	Invalid,
	Bool,
	Float,
	Double,
	Int32,
	Int64,
	Name,
	Byte,
	Class,
	Object,
	Struct
};

UENUM()
enum class EVoxelPropertyContainerType : uint8
{
	None,
	Array
};

USTRUCT()
struct VOXELCORE_API FVoxelPropertyType
{
	GENERATED_BODY()

private:
	UPROPERTY()
	EVoxelPropertyInternalType InternalType = EVoxelPropertyInternalType::Invalid;

	UPROPERTY()
	EVoxelPropertyContainerType ContainerType = EVoxelPropertyContainerType::None;

	UPROPERTY()
	TObjectPtr<UField> PrivateInternalField;

	FORCEINLINE UField* GetInternalField() const
	{
		return ResolveObjectPtrFast(PrivateInternalField);
	}

public:
	FVoxelPropertyType() = default;
	explicit FVoxelPropertyType(const FProperty& Property);

public:
	template<typename T>
	FORCEINLINE static FVoxelPropertyType Make()
	{
		checkStatic(TIsSafeVoxelPropertyType<T>::Value);

		if constexpr (std::is_same_v<T, bool>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Bool);
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Float);
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Double);
		}
		else if constexpr (std::is_same_v<T, int32>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Int32);
		}
		else if constexpr (std::is_same_v<T, int64>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Int64);
		}
		else if constexpr (std::is_same_v<T, FName>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Name);
		}
		else if constexpr (std::is_same_v<T, uint8>)
		{
			return MakeImpl(EVoxelPropertyInternalType::Byte);
		}
		else if constexpr (TIsEnum<T>::Value)
		{
			return FVoxelPropertyType::MakeImpl(EVoxelPropertyInternalType::Byte, StaticEnumFast<T>());
		}
		else if constexpr (TIsTSubclassOf<T>::Value)
		{
			return FVoxelPropertyType::MakeImpl(EVoxelPropertyInternalType::Class, StaticClassFast<typename TSubclassOfType<T>::Type>());
		}
		else if constexpr (IsObjectPtr<T>)
		{
			return FVoxelPropertyType::MakeImpl(EVoxelPropertyInternalType::Object, StaticClassFast<ObjectPtrInnerType<T>>());
		}
		else
		{
			return FVoxelPropertyType::MakeImpl(EVoxelPropertyInternalType::Struct, StaticStructFast<T>());
		}
	}

	FORCEINLINE static FVoxelPropertyType MakeEnum(UEnum* Enum)
	{
		checkVoxelSlow(Enum);
		return MakeImpl(EVoxelPropertyInternalType::Byte, Enum);
	}
	FORCEINLINE static FVoxelPropertyType MakeClass(UClass* BaseClass)
	{
		checkVoxelSlow(BaseClass);
		return MakeImpl(EVoxelPropertyInternalType::Class, BaseClass);
	}
	FORCEINLINE static FVoxelPropertyType MakeObject(UClass* Class)
	{
		checkVoxelSlow(Class);
		return MakeImpl(EVoxelPropertyInternalType::Object, Class);
	}
	static FVoxelPropertyType MakeStruct(UScriptStruct* Struct);
	static FVoxelPropertyType MakeFromK2(const FEdGraphPinType& PinType);

	static bool TryParse(
		const FString& TypeString,
		FVoxelPropertyType& OutType);

private:
	FORCEINLINE static FVoxelPropertyType MakeImpl(
		const EVoxelPropertyInternalType Type,
		UField* Field = nullptr)
	{
		FVoxelPropertyType PinType;
		PinType.InternalType = Type;
		PinType.PrivateInternalField = Field;
		checkVoxelSlow(PinType.IsValid());
		return PinType;
	}

public:
	bool IsValid() const;
	FString ToString() const;
	FEdGraphPinType GetEdGraphPinType_K2() const;
	bool CanBeCastedTo(const FVoxelPropertyType& Other) const;
#if WITH_EDITOR
	FSlateIcon GetIcon() const;
	FLinearColor GetColor() const;
#endif

public:
	template<typename T>
	FORCEINLINE bool Is() const
	{
		checkStatic(TIsSafeVoxelPropertyType<T>::Value);

		if constexpr (std::is_same_v<T, bool>)
		{
			return InternalType == EVoxelPropertyInternalType::Bool;
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			return InternalType == EVoxelPropertyInternalType::Float;
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			return InternalType == EVoxelPropertyInternalType::Double;
		}
		else if constexpr (std::is_same_v<T, int32>)
		{
			return InternalType == EVoxelPropertyInternalType::Int32;
		}
		else if constexpr (std::is_same_v<T, int64>)
		{
			return InternalType == EVoxelPropertyInternalType::Int64;
		}
		else if constexpr (std::is_same_v<T, FName>)
		{
			return InternalType == EVoxelPropertyInternalType::Name;
		}
		else if constexpr (std::is_same_v<T, uint8>)
		{
			return InternalType == EVoxelPropertyInternalType::Byte;
		}
		else if constexpr (TIsEnum<T>::Value)
		{
			checkVoxelSlow(StaticEnumFast<T>()->GetMaxEnumValue() <= MAX_uint8);
			return
				InternalType == EVoxelPropertyInternalType::Byte &&
				GetInternalField() == StaticEnumFast<T>();
		}
		else if constexpr (TIsTSubclassOf<T>::Value)
		{
			return
				InternalType == EVoxelPropertyInternalType::Class &&
				GetInternalField() == StaticClassFast<typename TSubclassOfType<T>::Type>();
		}
		else if constexpr (IsObjectPtr<T>)
		{
			return
				InternalType == EVoxelPropertyInternalType::Object &&
				GetInternalField() == StaticClassFast<ObjectPtrInnerType<T>>();
		}
		else
		{
			return
				InternalType == EVoxelPropertyInternalType::Struct &&
				GetInternalField() == StaticStructFast<T>();
		}
	}

	template<typename T>
	FORCEINLINE bool CanBeCastedTo() const
	{
		return this->CanBeCastedTo(Make<T>());
	}

public:
	FORCEINLINE EVoxelPropertyInternalType GetInternalType() const
	{
		return InternalType;
	}
	FORCEINLINE EVoxelPropertyContainerType GetContainerType() const
	{
		return ContainerType;
	}
	FORCEINLINE FVoxelPropertyType GetInnerType() const
	{
		FVoxelPropertyType Result = *this;
		Result.ContainerType = {};
		return Result;
	}

	FORCEINLINE void SetContainerType(const EVoxelPropertyContainerType NewContainerType)
	{
		ContainerType = NewContainerType;
	}

	FORCEINLINE bool IsClass() const
	{
		return InternalType == EVoxelPropertyInternalType::Class;
	}
	FORCEINLINE bool IsObject() const
	{
		return InternalType == EVoxelPropertyInternalType::Object;
	}
	FORCEINLINE bool IsStruct() const
	{
		return InternalType == EVoxelPropertyInternalType::Struct;
	}

	FORCEINLINE UEnum* GetEnum() const
	{
		checkVoxelSlow(InternalType == EVoxelPropertyInternalType::Byte);
		return CastChecked<UEnum>(GetInternalField(), ECastCheckedType::NullAllowed);
	}
	FORCEINLINE UClass* GetBaseClass() const
	{
		checkVoxelSlow(IsClass());
		return CastChecked<UClass>(GetInternalField(), ECastCheckedType::NullAllowed);
	}
	FORCEINLINE UClass* GetObjectClass() const
	{
		checkVoxelSlow(IsObject());
		return CastChecked<UClass>(GetInternalField(), ECastCheckedType::NullAllowed);
	}
	FORCEINLINE UScriptStruct* GetStruct() const
	{
		checkVoxelSlow(IsStruct());
		return CastChecked<UScriptStruct>(GetInternalField(), ECastCheckedType::NullAllowed);
	}

public:
	FORCEINLINE bool operator==(const FVoxelPropertyType& Other) const
	{
		return
			InternalType == Other.InternalType &&
			ContainerType == Other.ContainerType &&
			GetInternalField() == Other.GetInternalField();
	}
	FORCEINLINE bool operator!=(const FVoxelPropertyType& Other) const
	{
		return
			InternalType != Other.InternalType ||
			ContainerType != Other.ContainerType ||
			GetInternalField() != Other.GetInternalField();
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelPropertyType& Type)
	{
		return
			FVoxelUtilities::MurmurHash(int32(Type.InternalType) * 256 + int32(Type.ContainerType)) ^
			GetTypeHash(Type.GetInternalField());
	}
};
checkStatic(sizeof(FVoxelPropertyType) == 16);