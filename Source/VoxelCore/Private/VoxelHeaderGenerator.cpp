// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelHeaderGenerator.h"

#if WITH_EDITOR
#include "SourceCodeNavigation.h"
#include "VoxelShaderHooksManager.h"
#include "Interfaces/IPluginManager.h"
#endif

#if WITH_EDITOR
FVoxelHeaderRawContent& FVoxelHeaderRawContent::operator+=(const FString& Content)
{
	if (Content.IsEmpty())
	{
		Lines.Add({});
	}
	else
	{
		TArray<FString> NewLines;
		Content.ParseIntoArrayLines(NewLines, false);

		for (const FString& Line : NewLines)
		{
			if (Line.IsEmpty())
			{
				Lines.Add({});
			}

			Lines.Emplace(Line, Indentation);
		}
	}
	return *this;
}

FVoxelHeaderRawContent& FVoxelHeaderRawContent::operator++(int32)
{
	Indentation++;
	return *this;
}

FVoxelHeaderRawContent& FVoxelHeaderRawContent::operator--(int32)
{
	Indentation = FMath::Max(Indentation - 1, 0);
	return *this;
}

FString FVoxelHeaderRawContent::GenerateContent(const int32 InitialIndentation) const
{
	FString Result;
	for (const FData& Line : Lines)
	{
		if (Line.Content.IsEmpty())
		{
			Result += "\n";
		}
		else
		{
			if (Line.Content.StartsWith("#"))
			{
				Result += Line.Content + "\n";
			}
			else
			{
				Result += FString::ChrN(InitialIndentation + Line.Indentation, TCHAR('\t')) + Line.Content + "\n";
			}
		}
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelHeaderMetadata::Add(const bool bIsMeta, const FName& Key, const FString& Value, const FString& Separator)
{
	TArray<FName>& OrderedKeys = bIsMeta ? MetaOrderedKeys : DefaultOrderedKeys;
	TVoxelMap<FName, FString>& KeyToValue = bIsMeta ? MetaKeyToValue : DefaultKeyToValue;
	if (!KeyToValue.Contains(Key))
	{
		OrderedKeys.Add(Key);
	}

	FString& CurrentValue = KeyToValue.FindOrAdd(Key);
	if (!CurrentValue.IsEmpty())
	{
		CurrentValue += Separator;
	}

	CurrentValue += Value;
}

FString FVoxelHeaderMetadata::GenerateContent() const
{
	FString Data = GenerateString(DefaultOrderedKeys, DefaultKeyToValue);
	const FString Meta = GenerateString(MetaOrderedKeys, MetaKeyToValue);
	if (!Meta.IsEmpty())
	{
		if (!Data.IsEmpty())
		{
			Data += ", ";
		}

		Data += "meta = (" + Meta + ")";
	}

	if (bIsOptional &&
		Data.IsEmpty())
	{
		return {};
	}

	return Type + "(" + Data + ")";
}

FString FVoxelHeaderMetadata::GenerateString(const TArray<FName>& OrderedKeys, const TVoxelMap<FName, FString>& KeyToValue)
{
	FString Result;
	for (const FName Key : OrderedKeys)
	{
		const FString& Value = KeyToValue[Key];
		if (Key == "ToolTip" &&
			Value.IsEmpty())
		{
			continue;
		}

		if (!Result.IsEmpty())
		{
			Result += ", ";
		}

		Result += Key.ToString();
		if (!Value.IsEmpty())
		{
			Result += " = \"" + Value.ReplaceQuotesWithEscapedQuotes() + "\"";
		}
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeaderFunctionArgument::FVoxelHeaderFunctionArgument(const FString& Name, const FString& Type)
	: Name(Name)
	, Type(Type)
{
	Metadata.bIsOptional = true;
}

FVoxelHeaderFunctionArgument FVoxelHeaderFunctionArgument::Make(const FString& Name, const FString& Type)
{
	return FVoxelHeaderFunctionArgument(Name, Type);
}

FVoxelHeaderFunctionArgument FVoxelHeaderFunctionArgument::Make(const FProperty& Property, const FString& OverrideName)
{
	FVoxelHeaderFunctionArgument Result(Property.GetNameCPP(), FVoxelUtilities::GetFunctionType(Property));
	if (!OverrideName.IsEmpty())
	{
		Result.Name = OverrideName;
	}

	Result.AddMetadata(false, "DisplayName", Property.GetDisplayNameText().ToString());
	Result.AddMetadata(true, "ToolTip", Property.GetToolTipText().ToString().ReplaceCharWithEscapedChar());

	if (const FString* AllowedClassesPtr = Property.FindMetaData("AllowedClasses"))
	{
		Result.AddMetadata(true, "AllowedClasses", *AllowedClassesPtr);
	}

	return Result;
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunctionArgument::AddMetadata(const bool bIsMeta, const FString& Key, const FString& Value, const FString& Separator)
{
	Metadata.Add(bIsMeta, FName(Key), Value, Separator);
	return *this;
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunctionArgument::SetDefault(const FString& NewDefault)
{
	Default = NewDefault;
	return *this;
}

FString FVoxelHeaderFunctionArgument::GenerateContent(const bool bFunctionUsesUHT) const
{
	FString Result;

	if (bFunctionUsesUHT)
	{
		FVoxelHeaderMetadata CopiedMetadata = Metadata;
		if (const FString* DisplayName = CopiedMetadata.DefaultKeyToValue.Find("DisplayName"))
		{
			if (*DisplayName == Name ||
				*DisplayName == FName::NameToDisplayString(Name, Type == "bool"))
			{
				CopiedMetadata.DefaultKeyToValue.Remove("DisplayName");
				CopiedMetadata.DefaultOrderedKeys.Remove("DisplayName");
			}
		}

		Result += CopiedMetadata.GenerateContent();
		if (!Result.IsEmpty())
		{
			Result += " ";
		}
	}

	if (bConst)
	{
		Result += "const ";
	}
	Result += Type;
	if (bRef)
	{
		Result += "&";
	}
	if (bPointer)
	{
		Result += "*";
	}
	Result += " " + Name;

	if (!Default.IsEmpty())
	{
		Result += " = " + Default;
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeaderFunction::FVoxelHeaderFunction(
	const FString& FunctionName,
	const bool bUseUHT)
	: FunctionName(FunctionName)
	, bUseUHT(bUseUHT)
{
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunction::AddArgument(const FString& ArgName, const FString& Type)
{
	return Arguments.Add_GetRef(FVoxelHeaderFunctionArgument::Make(ArgName, Type));
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunction::AddArgument(const FProperty& Property, const FString& OverrideName)
{
	const FVoxelHeaderFunctionArgument NewArgument = FVoxelHeaderFunctionArgument::Make(Property, OverrideName);
	if (CastField<FArrayProperty>(Property) ||
		CastField<FMapProperty>(Property) ||
		CastField<FSetProperty>(Property))
	{
		AddMetadata(true, "AutoCreateRefTerm", NewArgument.Name);
	}

	return Arguments.Add_GetRef(NewArgument);
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunction::AddArgument(const FVoxelHeaderFunctionArgument& Param)
{
	return Arguments.Add_GetRef(Param);
}

FVoxelHeaderFunctionArgument& FVoxelHeaderFunction::AddArgumentWithDefault(const FProperty& Property, const void* ContainerData, const UObject* Owner, const FString& OverrideName)
{
	const FVoxelHeaderFunctionArgument NewArgument = FVoxelHeaderFunctionArgument::Make(Property, OverrideName);
	INLINE_LAMBDA
	{
		if (CastField<FArrayProperty>(Property) ||
			CastField<FMapProperty>(Property) ||
			CastField<FSetProperty>(Property))
		{
			AddMetadata(true, "AutoCreateRefTerm", NewArgument.Name);
			return;
		}

		const void* ContainerValue = Property.ContainerPtrToValuePtr<void>(ContainerData, 0);
		void* DefaultValue = Property.AllocateAndInitializeValue();
		ON_SCOPE_EXIT
		{
			Property.DestroyAndFreeValue(DefaultValue);
		};

		if (Property.Identical(ContainerValue, DefaultValue))
		{
			return;
		}

		FString Value = FVoxelUtilities::PropertyToText_InContainer(Property, ContainerData, Owner);
		// FVector and FRotator default value has to be without parenthesis and variable names
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct)
			{
				if (StructProperty->Struct == TBaseStructure<FVector>::Get())
				{
					Value.RemoveFromStart("(");
					Value.RemoveFromEnd(")");
					Value.ReplaceInline(TEXT("X="), TEXT(""));
					Value.ReplaceInline(TEXT("Y="), TEXT(""));
					Value.ReplaceInline(TEXT("Z="), TEXT(""));
				}
				else if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
				{
					Value.RemoveFromStart("(");
					Value.RemoveFromEnd(")");
					Value.ReplaceInline(TEXT("Pitch="), TEXT(""));
					Value.ReplaceInline(TEXT("Yaw="), TEXT(""));
					Value.ReplaceInline(TEXT("Roll="), TEXT(""));
				}
			}
		}
		
		if (Value.IsEmpty())
		{
			return;
		}

		AddMetadata(true, NewArgument.Name, Value);
	};

	return Arguments.Add_GetRef(NewArgument);
}

void FVoxelHeaderFunction::AddMetadata(const bool bIsMeta, const FString& Key, const FString& Value, const FString& Separator)
{
	Metadata.Add(bIsMeta, FName(Key), Value, Separator);
}

void FVoxelHeaderFunction::AddComment(const FString& Content)
{
	TArray<FString> NewLines;
	Content.ParseIntoArrayLines(NewLines, false);

	for (const FString& Line : NewLines)
	{
		Comment.Add(Line);
	}
}

FVoxelHeaderFunction& FVoxelHeaderFunction::operator+=(const FString& Content)
{
	FunctionBody += Content;
	return *this;
}

FVoxelHeaderFunction& FVoxelHeaderFunction::operator++(int32)
{
	FunctionBody++;
	return *this;
}

FVoxelHeaderFunction& FVoxelHeaderFunction::operator--(int32)
{
	FunctionBody--;
	return *this;
}

FString FVoxelHeaderFunction::GenerateContent(const bool bObjectUsesUHT) const
{
	FString Result;

	for (const FString& CommentLine : Comment)
	{
		Result += "\t// " + CommentLine + "\n";
	}

	const bool bUHT =
		bUseUHT &&
		bObjectUsesUHT;

	if (bUHT)
	{
		Result += "\t" + Metadata.GenerateContent() + "\n";
	}

	Result += "\t";
	if (bStatic)
	{
		Result += "static ";
	}
	Result += FunctionReturnType + " " + FunctionName + "(";
	if (Arguments.Num() == 1)
	{
		Result += Arguments[0].GenerateContent(bUHT);
	}
	else if (Arguments.Num() > 1)
	{
		Result += "\n";
		for (int32 Index = 0; Index < Arguments.Num(); Index++)
		{
			Result += "\t\t" + Arguments[Index].GenerateContent(bUHT);
			if (Index + 1 < Arguments.Num())
			{
				Result += ",\n";
			}
		}
	}
	Result += ")";
	Result += "\n";
	Result += "\t{\n";

	Result += FunctionBody.GenerateContent(2);

	Result += "\t}\n";
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeaderObject::FVoxelHeaderObject(const FString& Name, const bool bIsClass, const bool bUseUHT)
	: Name(Name)
	, bUseUHT(bUseUHT)
	, bIsClass(bIsClass)
{
	Metadata.Type = bIsClass ? "UCLASS" : "USTRUCT";
}

void FVoxelHeaderObject::AddParent(const FString& InName)
{
	Parents.Add(InName);
}

void FVoxelHeaderObject::AddParent(const UStruct* Struct)
{
	if (!ensure(Struct))
	{
		return;
	}

	Parents.Add(Struct->GetPrefixCPP() + Struct->GetName());

	FString HeaderPath;
	if (FVoxelHeaderGenerator::GetPath(Struct, HeaderPath))
	{
		ParentIncludes.Add(HeaderPath);
	}
}

void FVoxelHeaderObject::AddMetadata(const bool bIsMeta, const FString& Key, const FString& Value, const FString& Separator)
{
	Metadata.Add(bIsMeta, FName(Key), Value, Separator);
}

void FVoxelHeaderObject::AddTemplate(const FString& Template)
{
	Templates.Add(Template);
}

FVoxelHeaderFunction& FVoxelHeaderObject::AddFunction(const FString& FuncName, const bool bFuncUseUHT)
{
	ensure(!bFuncUseUHT || bUseUHT);
	return Functions.Add_GetRef(FVoxelHeaderFunction(FuncName, bFuncUseUHT));
}

FVoxelHeaderObject& FVoxelHeaderObject::operator+=(const FString& Content)
{
	ObjectBody += Content;
	return *this;
}

FVoxelHeaderObject& FVoxelHeaderObject::operator++(int32)
{
	ObjectBody++;
	return *this;
}

FVoxelHeaderObject& FVoxelHeaderObject::operator--(int32)
{
	ObjectBody--;
	return *this;
}

FString FVoxelHeaderObject::GenerateContent(const FString& API) const
{
	FString Result;

	FString Prefix;
	if (Templates.Num() > 0)
	{
		FString TemplatesContent;
		for (const FString& Template : Templates)
		{
			if (!TemplatesContent.IsEmpty())
			{
				TemplatesContent += ", ";
			}
			TemplatesContent += Template;
		}
		
		Result += "template<" + TemplatesContent + ">\n";

		if (bIsClass)
		{
			Prefix = "class ";
		}
		else
		{
			Prefix = "struct ";
		}
	}
	else
	{
		if (bUseUHT)
		{
			if (bIsClass)
			{
				Result += Metadata.GenerateContent() + "\n";
				Prefix = "class " + API + " U";
			}
			else
			{
				Result += Metadata.GenerateContent() + "\n";
				Prefix = "struct " + API + " F";
			}
		}
		else
		{
			if (bIsClass)
			{
				Prefix = "class " + API + " F";
			}
			else
			{
				Prefix = "struct " + API + " F";
			}
		}
	}

	Result += Prefix + Name;
	if (bFinal)
	{
		Result += " final";
	}

	if (Parents.Num() == 0)
	{
		Result += "\n";
	}
	else if (Parents.Num() == 1)
	{
		Result += " : public " + Parents[0] + "\n";
	}
	else
	{
		Result += "\n";
		for (int32 Index = 0; Index < Parents.Num(); Index++)
		{
			Result += "\t";
			Result += Index == 0 ? ":" : ",";
			Result += " public " + Parents[Index] + "\n";
		}
	}

	Result += "{\n";

	if (bUseUHT)
	{
		Result += "\tGENERATED_BODY()\n";
	}

	Result += ObjectBody.GenerateContent(1) + "\n";

	if (Functions.Num() > 0 &&
		bIsClass)
	{
		Result += "public:\n";
	}
	else if (Functions.Num() == 0)
	{
		Result.RemoveFromEnd("\n");
	}

	for (int32 Index = 0; Index < Functions.Num(); Index++)
	{
		Result += Functions[Index].GenerateContent(bUseUHT);
		if (Index + 1 < Functions.Num())
		{
			Result += "\n";
		}
	}

	Result += "};";
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeaderGenerator::FVoxelHeaderGenerator(
	const FString& Name,
	const FString& Path,
	const FString& API)
	: Path(Path)
	, Name(Name)
	, API(API)
{
}

FVoxelHeaderGenerator::FVoxelHeaderGenerator(
	const FString& Name,
	const UStruct* Struct)
	: Name(Name)
{
	if (!Struct)
	{
		return;
	}

	FString HeaderPath;
	if (!FSourceCodeNavigation::FindClassHeaderPath(Struct, HeaderPath))
	{
		return;
	}

	Path = FPaths::GetPath(FPaths::ConvertRelativePathToFull(HeaderPath));
	API = Struct->GetOutermost()->GetName().Replace(TEXT("/Script/"), TEXT("")).ToUpper() + "_API";
}

void FVoxelHeaderGenerator::AddInclude(const FString& IncludePath)
{
	Includes.Add(IncludePath);
}

void FVoxelHeaderGenerator::AddInclude(const UStruct* Struct)
{
	FString HeaderPath;
	if (GetPath(Struct, HeaderPath))
	{
		Includes.Add(HeaderPath);
	}
}

FVoxelHeaderObject& FVoxelHeaderGenerator::AddObject(const FString& ObjectName, const bool bIsClass, const bool bUseUHT)
{
	return Objects.Add_GetRef(FVoxelHeaderObject(ObjectName, bIsClass, bUseUHT));
}

FVoxelHeaderObject& FVoxelHeaderGenerator::AddClass(const FString& ObjectName, const bool bUseUHT)
{
	return AddObject(ObjectName, true, bUseUHT);
}

FVoxelHeaderObject& FVoxelHeaderGenerator::AddStruct(const FString& ObjectName, const bool bUseUHT)
{
	return AddObject(ObjectName, false, bUseUHT);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelHeaderGenerator::GetHeaderName(const UStruct* Struct, FString& OutName)
{
	FString HeaderPath;
	if (!FSourceCodeNavigation::FindClassHeaderPath(Struct, HeaderPath))
	{
		return false;
	}

	OutName = FPaths::GetBaseFilename(HeaderPath);
	return true;
}

bool FVoxelHeaderGenerator::GetPath(const UStruct* Struct, FString& OutPath)
{
	FString HeaderPath;
	if (!FSourceCodeNavigation::FindClassHeaderPath(Struct, HeaderPath))
	{
		return false;
	}

	FString ModulePath;
	FSourceCodeNavigation::FindModulePath(Struct->GetPackage(), ModulePath);

	FPaths::MakePathRelativeTo(HeaderPath, *ModulePath);

	int32 Index = -1;
	// Remove module directory
	if (HeaderPath.FindChar('/', Index))
	{
		HeaderPath.RightChopInline(Index + 1);
	}
	// Remove public/private directory
	if (HeaderPath.FindChar('/', Index))
	{
		HeaderPath.RightChopInline(Index + 1);
	}

	OutPath = HeaderPath;
	return true;
}

bool FVoxelHeaderGenerator::CreateFile() const
{
	const FString FilePath = Path / Name + ".h";

	FString ExistingFile;
	FFileHelper::LoadFileToString(ExistingFile, *FilePath);

	// Normalize line endings
	ExistingFile.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

	const FString LibraryFile = GenerateHeader();
	if (ExistingFile.Equals(LibraryFile))
	{
		return false;
	}

	IFileManager::Get().Delete(*FilePath, false, true);
	ensure(FFileHelper::SaveStringToFile(LibraryFile, *FilePath));
	LOG_VOXEL(Error, "%s written", *FilePath);
	return true;
}

FString FVoxelHeaderGenerator::GenerateHeader() const
{
	FString LibraryFile;
	LibraryFile += "// Copyright Voxel Plugin SAS. All Rights Reserved.\n";
	LibraryFile += "\n";
	LibraryFile += "#pragma once\n";
	LibraryFile += "\n";

	if (bIsEditor)
	{
		LibraryFile += "#include \"VoxelEditorMinimal.h\"\n";
	}
	else
	{
		LibraryFile += "#include \"VoxelMinimal.h\"\n";
	}

	{
		TSet<FString> FinalIncludes = Includes;
		for (const FVoxelHeaderObject& Object : Objects)
		{
			FinalIncludes.Append(Object.ParentIncludes);
		}

		TArray<FString> OrderedIncludes = FinalIncludes.Array();
		OrderedIncludes.Sort([](const FString& A, const FString& B)
		{
			return A.Len() < B.Len();
		});

		for (const FString& Include : OrderedIncludes)
		{
			LibraryFile += "#include \"" + Include + "\"\n";
		}
	}

	bool bUsesUHT = false;
	for (const FVoxelHeaderObject& Object : Objects)
	{
		if (Object.bUseUHT)
		{
			bUsesUHT = true;
		}
	}

	if (bUsesUHT)
	{
		LibraryFile += "#include \"" + Name + ".generated.h\"\n";
	}

	LibraryFile += "\n";
	LibraryFile += "////////////////////////////////////////////////////\n";
	LibraryFile += "///////// The code below is auto-generated /////////\n";
	LibraryFile += "////////////////////////////////////////////////////\n";
	LibraryFile += "\n";

	for (int32 Index = 0; Index < Objects.Num(); Index++)
	{
		LibraryFile += Objects[Index].GenerateContent(API);
		if (Index + 1 < Objects.Num())
		{
			LibraryFile += "\n\n";
		}
	}

	return LibraryFile;
}
#endif