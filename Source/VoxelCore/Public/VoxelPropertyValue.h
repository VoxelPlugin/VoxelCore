// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelPropertyValueBase.h"
#include "VoxelPropertyValue.generated.h"

USTRUCT()
struct VOXELCORE_API FVoxelPropertyTerminalValue final : public FVoxelPropertyValueBase
{
	GENERATED_BODY()

public:
	FVoxelPropertyTerminalValue() = default;
	explicit FVoxelPropertyTerminalValue(const FVoxelPropertyType& Type);

private:
	explicit FVoxelPropertyTerminalValue(const FVoxelPropertyValueBase& Value)
		: FVoxelPropertyValueBase((Value))
	{
	}
	explicit FVoxelPropertyTerminalValue(FVoxelPropertyValueBase&& Value)
		: FVoxelPropertyValueBase((MoveTemp(Value)))
	{
	}

public:
	FVoxelPropertyValue AsValue() const;

	FORCEINLINE TVoxelArrayView<uint8> GetRawView()
	{
		switch (Type.GetInternalType())
		{
		default: VOXEL_ASSUME(false);
		case EVoxelPropertyInternalType::Bool: return MakeByteVoxelArrayView(bBool);
		case EVoxelPropertyInternalType::Float: return MakeByteVoxelArrayView(Float);
		case EVoxelPropertyInternalType::Double: return MakeByteVoxelArrayView(Double);
		case EVoxelPropertyInternalType::Int32: return MakeByteVoxelArrayView(Int32);
		case EVoxelPropertyInternalType::Int64: return MakeByteVoxelArrayView(Int64);
		case EVoxelPropertyInternalType::Name: return MakeByteVoxelArrayView(Name);
		case EVoxelPropertyInternalType::Byte: return MakeByteVoxelArrayView(Byte);
		case EVoxelPropertyInternalType::Class: return MakeByteVoxelArrayView(Class);
		case EVoxelPropertyInternalType::Object: return MakeByteVoxelArrayView(Object);
		case EVoxelPropertyInternalType::Struct: return Struct.GetStructView();
		}
	}
	FORCEINLINE TConstVoxelArrayView<uint8> GetRawView() const
	{
		return ConstCast(this)->GetRawView();
	}

public:
	template<typename T, typename = std::enable_if_t<
		TIsSafeVoxelPropertyValue<T>::Value &&
		!std::derived_from<T, UObject>>>
	static FVoxelPropertyTerminalValue Make(const T& Value = FVoxelUtilities::MakeSafe<T>())
	{
		FVoxelPropertyTerminalValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}
	template<typename T> requires(std::derived_from<T, UObject>)
	static FVoxelPropertyTerminalValue Make(T* Value = nullptr)
	{
		FVoxelPropertyTerminalValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}
	template<typename T> requires(std::derived_from<T, UObject>)
	static FVoxelPropertyTerminalValue Make(const TObjectPtr<T>& Value)
	{
		FVoxelPropertyTerminalValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}

	static FVoxelPropertyTerminalValue MakeStruct(const FConstVoxelStructView Struct)
	{
		return FVoxelPropertyTerminalValue(Super::MakeStruct(Struct));
	}
	static FVoxelPropertyTerminalValue MakeFromProperty(const FProperty& Property, const void* Memory)
	{
		return FVoxelPropertyTerminalValue(Super::MakeFromProperty(Property, Memory));
	}

public:
	bool operator==(const FVoxelPropertyTerminalValue& Other) const
	{
		return Super::operator==(Other);
	}

	friend uint32 GetTypeHash(const FVoxelPropertyTerminalValue& Value)
	{
		return Value.GetHash();
	}

	friend struct FVoxelPropertyValue;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct VOXELCORE_API FVoxelPropertyValue final : public FVoxelPropertyValueBase
{
	GENERATED_BODY()

public:
	FVoxelPropertyValue() = default;
	explicit FVoxelPropertyValue(const FVoxelPropertyType& Type);

	template<typename T> requires(!std::derived_from<T, UObject> && TIsSafeVoxelPropertyValue<T>::Value)
	static FVoxelPropertyValue Make(const T& Value = FVoxelUtilities::MakeSafe<T>())
	{
		FVoxelPropertyValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}
	template<typename T> requires(std::derived_from<T, UObject>)
	static FVoxelPropertyValue Make(T* Value = nullptr)
	{
		FVoxelPropertyValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}
	template<typename T, typename = std::enable_if_t<std::derived_from<T, UObject>>>
	static FVoxelPropertyValue Make(const TObjectPtr<T>& Value)
	{
		FVoxelPropertyValue Result(FVoxelPropertyType::Make<T>());
		Result.Get<T>() = Value;
		return Result;
	}

	static FVoxelPropertyValue MakeFromK2PinDefaultValue(const UEdGraphPin& Pin);

	static FVoxelPropertyValue MakeStruct(FConstVoxelStructView Struct);
	static FVoxelPropertyValue MakeFromProperty(const FProperty& Property, const void* Memory);

	using FVoxelPropertyValueBase::Fixup;
	void Fixup(const FVoxelPropertyType& NewType);
	bool ImportFromUnrelated(FVoxelPropertyValue Other);
	FVoxelPropertyTerminalValue AsTerminalValue() const;

public:
	FORCEINLINE bool IsArray() const
	{
		return Type.GetContainerType() == EVoxelPropertyContainerType::Array;
	}

	FORCEINLINE const TArray<FVoxelPropertyTerminalValue>& GetArray() const
	{
		checkVoxelSlow(IsArray());
		return Array;
	}
	FORCEINLINE void AddValue(const FVoxelPropertyTerminalValue& InnerValue)
	{
		checkVoxelSlow(IsArray());
		checkVoxelSlow(Type.GetInnerType() == InnerValue.GetType());
		Array.Add(InnerValue);
	}

private:
	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<FVoxelPropertyTerminalValue> Array;

	explicit FVoxelPropertyValue(const FVoxelPropertyValueBase& Value)
		: FVoxelPropertyValueBase((Value))
	{
	}
	explicit FVoxelPropertyValue(FVoxelPropertyValueBase&& Value)
		: FVoxelPropertyValueBase((MoveTemp(Value)))
	{
	}

	virtual bool HasArray() const override { return true; }
	virtual FString ExportToString_Array() const override;
	virtual void ExportToProperty_Array(const FProperty& Property, void* Memory) const override;
	virtual bool ImportFromString_Array(const FString& Value) override;
	virtual uint32 GetHash_Array() const override;
	virtual void Fixup_Array() override;
	virtual bool Equal_Array(const FVoxelPropertyValueBase& Other) const override;

public:
	bool operator==(const FVoxelPropertyValue& Other) const
	{
		return Super::operator==(Other);
	}

	friend uint32 GetTypeHash(const FVoxelPropertyValue& Value)
	{
		return Value.GetHash();
	}

	friend FVoxelPropertyTerminalValue;
	friend class FVoxelParameterDetails;
	friend class FVoxelPropertyValueCustomization;
	friend class FVoxelPropertyCustomizationUtilities;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<>
struct TStructOpsTypeTraits<FVoxelPropertyTerminalValue> : public TStructOpsTypeTraitsBase2<FVoxelPropertyTerminalValue>
{
	enum
	{
		WithSerializer = true,
		WithPostSerialize = true,
	};
};

template<>
struct TStructOpsTypeTraits<FVoxelPropertyValue> : public TStructOpsTypeTraitsBase2<FVoxelPropertyValue>
{
	enum
	{
		WithSerializer = true,
		WithPostSerialize = true,
	};
};