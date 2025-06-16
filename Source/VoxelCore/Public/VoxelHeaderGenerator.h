// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

#if WITH_EDITOR
struct VOXELCORE_API FVoxelHeaderRawContent
{
	FVoxelHeaderRawContent& operator+=(const FString& Content);
	FVoxelHeaderRawContent& operator++(int32);
	FVoxelHeaderRawContent& operator--(int32);

	FString GenerateContent(int32 InitialIndentation) const;

private:
	struct FData
	{
		FString Content;
		int32 Indentation = 0;
	};
	TArray<FData> Lines;
	int32 Indentation = 0;
};

struct VOXELCORE_API FVoxelHeaderMetadata
{
public:
	FVoxelHeaderMetadata() = default;
	FVoxelHeaderMetadata(const FString& Type)
		: Type(Type)
	{}

	void Add(bool bIsMeta, const FName& Key, const FString& Value, const FString& Separator = ", ");

public:
	FString GenerateContent() const;

private:
	static FString GenerateString(const TArray<FName>& OrderedKeys, const TVoxelMap<FName, FString>& KeyToValue);

public:
	bool bIsOptional = false;
	FString Type;

	TVoxelMap<FName, FString> DefaultKeyToValue;
	TArray<FName> DefaultOrderedKeys;
	TVoxelMap<FName, FString> MetaKeyToValue;
	TArray<FName> MetaOrderedKeys;
};

struct VOXELCORE_API FVoxelHeaderFunctionArgument
{
private:
	FVoxelHeaderFunctionArgument(const FString& Name, const FString& Type);

public:
	static FVoxelHeaderFunctionArgument Make(const FString& Name, const FString& Type);
	static FVoxelHeaderFunctionArgument Make(const FProperty& Property, const FString& OverrideName = "");
	template<typename T>
	static FVoxelHeaderFunctionArgument Make(const FString& Name)
	{
		return FVoxelHeaderFunctionArgument(Name, FVoxelUtilities::GetCppName<T>());
	}

	FVoxelHeaderFunctionArgument& Const(const bool bInConst = true)
	{
		bConst = bInConst;
		return *this;
	}
	FVoxelHeaderFunctionArgument& Ref(const bool bInRef = true)
	{
		bRef = bInRef;
		return *this;
	}
	FVoxelHeaderFunctionArgument& Pointer(const bool bInPointer = true)
	{
		bPointer = bInPointer;
		return *this;
	}
	FVoxelHeaderFunctionArgument& AddMetadata(bool bIsMeta, const FString& Key, const FString& Value = "", const FString& Separator = ", ");
	FVoxelHeaderFunctionArgument& SetDefault(const FString& NewDefault);

public:
	FString GenerateContent(bool bFunctionUsesUHT) const;

public:
	FString Name;
	FString Type;
	bool bConst = false;
	bool bRef = false;
	bool bPointer = false;
	FString Default;
	FVoxelHeaderMetadata Metadata = FVoxelHeaderMetadata("UPARAM");
	friend struct FVoxelHeaderFunction;
};

struct VOXELCORE_API FVoxelHeaderFunction
{
public:
	FVoxelHeaderFunction(const FString& Name, bool bUseUHT = true);

	void ReturnType(const FString& Type)
	{
		FunctionReturnType = Type;
	}
	void ReturnType(const FProperty& Property)
	{
		FunctionReturnType = Property.GetCPPType();
	}
	template<typename T>
	void ReturnType()
	{
		FunctionReturnType = FVoxelUtilities::GetCppName<T>();
	}

	FVoxelHeaderFunctionArgument& AddArgument(const FString& ArgName, const FString& Type);
	FVoxelHeaderFunctionArgument& AddArgument(const FProperty& Property, const FString& OverrideName = "");
	FVoxelHeaderFunctionArgument& AddArgument(const FVoxelHeaderFunctionArgument& Param);
	template<typename T>
	FVoxelHeaderFunctionArgument& AddArgument(const FString& Name)
	{
		return AddArgument(Name, FVoxelUtilities::GetCppName<T>());
	}

	FVoxelHeaderFunctionArgument& AddArgumentWithDefault(const FProperty& Property, const void* ContainerData, const UObject* Owner, const FString& OverrideName = "");

	void AddMetadata(bool bIsMeta, const FString& Key, const FString& Value = "", const FString& Separator = ", ");
	void AddComment(const FString& Content);

	FVoxelHeaderFunction& operator+=(const FString& Content);
	FVoxelHeaderFunction& operator++(int32);
	FVoxelHeaderFunction& operator--(int32);

private:
	FString GenerateContent(bool bObjectUsesUHT) const;

public:
	FString Name;
	bool bUseUHT;
	FVoxelHeaderMetadata Metadata = FVoxelHeaderMetadata("UFUNCTION");

	FString FunctionReturnType = "void";
	bool bStatic = true;

private:
	TArray<FVoxelHeaderFunctionArgument> Arguments;
	FVoxelHeaderRawContent FunctionBody;

	TArray<FString> Comment;

	friend struct FVoxelHeaderObject;
};

struct VOXELCORE_API FVoxelHeaderObject
{
public:
	FVoxelHeaderObject(const FString& Name, bool bIsClass, bool bUseUHT);

	void AddParent(const FString& InName);
	void AddParent(const UStruct* Struct);
	template<typename T>
	void AddParent()
	{
		if constexpr (TModels_V<CStaticClassProvider, T>)
		{
			AddParent(T::StaticClass());
		}
		else if constexpr (TModels_V<CStaticStructProvider, T>)
		{
			AddParent(StaticStruct<T>());
		}
		else
		{
			static_assert(sizeof(T) == 0, "Unsupported type.");
		}
	}

	void SetFinal(const bool bInFinal)
	{
		bFinal = bInFinal;
	}

	void AddMetadata(bool bIsMeta, const FString& Key, const FString& Value = "", const FString& Separator = ", ");
	void AddTemplate(const FString& Template);

	FVoxelHeaderFunction& AddFunction(const FString& FuncName, bool bFuncUseUHT = true);

	FVoxelHeaderObject& operator+=(const FString& Content);
	FVoxelHeaderObject& operator++(int32);
	FVoxelHeaderObject& operator--(int32);

private:
	FString GenerateContent(const FString& API) const;

public:
	FString Name;
	bool bFinal = false;
	bool bUseUHT = false;

private:
	bool bIsClass = true;
	TArray<FString> Parents;
	TSet<FString> ParentIncludes;
	FVoxelHeaderMetadata Metadata;
	TArray<FVoxelHeaderFunction> Functions;
	TArray<FString> Templates;

	FVoxelHeaderRawContent ObjectBody;

	friend struct FVoxelHeaderGenerator;
};

struct VOXELCORE_API FVoxelHeaderGenerator
{
public:
	FVoxelHeaderGenerator(
		const FString& Name,
		const FString& Path,
		const FString& API);
	FVoxelHeaderGenerator(
		const FString& Name,
		const UStruct* Struct);

	void AddInclude(const FString& IncludePath);
	void AddInclude(const UStruct* Struct);
	template<typename T>
	void AddInclude()
	{
		if constexpr (TModels_V<CStaticClassProvider, T>)
		{
			AddInclude(T::StaticClass());
		}
		else if constexpr (TModels_V<CStaticStructProvider, T>)
		{
			AddInclude(StaticStruct<T>());
		}
		else
		{
			static_assert(sizeof(T) == 0, "Unsupported type.");
		}
	}

	FVoxelHeaderObject& AddObject(const FString& ObjectName, bool bIsClass, bool bUseUHT = true);
	FVoxelHeaderObject& AddClass(const FString& ObjectName, bool bUseUHT = true);
	FVoxelHeaderObject& AddStruct(const FString& ObjectName, bool bUseUHT = true);

	static bool GetHeaderName(const UStruct* Struct, FString& OutName);
	static bool GetPath(const UStruct* Struct, FString& OutPath);

public:
	bool CreateFile() const;

private:
	FString GenerateHeader() const;

public:
	FString Path;
	FString Name;
	FString API;
	bool bIsEditor = false;

private:
	TArray<FVoxelHeaderObject> Objects;
	TSet<FString> Includes;
};
#endif