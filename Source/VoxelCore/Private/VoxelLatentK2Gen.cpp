// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#if WITH_EDITOR
#include "SourceCodeNavigation.h"
#endif

#if WITH_EDITOR
VOXEL_RUN_ON_STARTUP_GAME()
{
	UScriptStruct* FutureStruct = FindObjectChecked<UScriptStruct>(nullptr, TEXT("/Script/VoxelCore.VoxelFuture"));
	check(FutureStruct);
	FutureStruct->SetMetaData(TEXT("BlueprintType"), TEXT("false"));

	{
		VOXEL_SCOPE_COUNTER("Iterate FVoxelLatentContext");

		for (const UClass* Class : GetDerivedClasses<UBlueprintFunctionLibrary>())
		{
			for (UFunction* Function : GetClassFunctions(Class))
			{
				const FStructProperty* Property = CastField<FStructProperty>(Function->GetReturnProperty());
				if (!Property)
				{
					continue;
				}

				if (Property->Struct == FutureStruct)
				{
					EnumRemoveFlags(Function->FunctionFlags, FUNC_BlueprintCallable);
				}
			}
		}
	}

	if (!FVoxelUtilities::IsDevWorkflow())
	{
		return;
	}

	TVoxelMap<const UClass*, TVoxelArray<const UFunction*>> ClassToFunctions;
	for (const UClass* Class : GetDerivedClasses<UBlueprintFunctionLibrary>())
	{
		TVoxelArray<const UFunction*> Functions;
		for (const UFunction* Function : GetClassFunctions(Class))
		{
			const FStructProperty* Property = CastField<FStructProperty>(Function->GetReturnProperty());
			if (!Property)
			{
				continue;
			}

			if (Property->Struct == FutureStruct)
			{
				ensure(!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
				Functions.Add(Function);
			}
		}

		if (Functions.Num() > 0)
		{
			ClassToFunctions.Add_EnsureNew(Class, MoveTemp(Functions));
		}
	}

	ClassToFunctions.KeySort([](const UClass* A, const UClass* B)
	{
		return A->GetName() < B->GetName();
	});

	TVoxelMap<FString, TVoxelArray<const UClass*>> HeaderToClasses;
	TVoxelMap<const UClass*, FString> ClassToHeader;

	for (const auto& It : ClassToFunctions)
	{
		const UClass* Class = It.Key;

		FString Header;
		if (!FSourceCodeNavigation::FindClassHeaderPath(Class, Header))
		{
			continue;
		}

		HeaderToClasses.FindOrAdd(Header).Add(Class);
		ClassToHeader.Add_EnsureNew(Class, Header);
	}

	bool bModified = false;
	ON_SCOPE_EXIT
	{
		if (bModified)
		{
			ensure(false);
			FPlatformMisc::RequestExit(true);
		}
	};

	for (const auto& HeaderIt : HeaderToClasses)
	{
		const FString Header = HeaderIt.Key;
		const FString HeaderName = FPaths::GetBaseFilename(Header);
		const TVoxelArray<const UClass*>& Classes = HeaderIt.Value;

		FString GeneratedFile;
		GeneratedFile += "// Copyright Voxel Plugin SAS. All Rights Reserved.\n";
		GeneratedFile += "\n";
		GeneratedFile += "#pragma once\n";
		GeneratedFile += "\n";
		GeneratedFile += "#include \"VoxelMinimal.h\"\n";
		GeneratedFile += "#include \"VoxelLatentAction.h\"\n";
		GeneratedFile += "#include \"" + HeaderName + ".h\"\n";
		GeneratedFile += "#include \"" + HeaderName + "_K2.generated.h\"\n";
		GeneratedFile += "\n";
		GeneratedFile += "////////////////////////////////////////////////////\n";
		GeneratedFile += "///////// The code below is auto-generated /////////\n";
		GeneratedFile += "////////////////////////////////////////////////////\n";
		GeneratedFile += "\n";

		for (const UClass* Class : Classes)
		{
			const FString Name = Class->GetName();
			const FString DisplayName = Class->GetDisplayNameText().ToString();

			FString NameWithoutVoxel = Name;
			NameWithoutVoxel.RemoveFromStart("Voxel");

			const FString DisplayNameWithoutVoxel = FName::NameToDisplayString(NameWithoutVoxel, false);

			const FString Api = Class->GetOutermost()->GetName().Replace(TEXT("/Script/"), TEXT("")).ToUpper() + "_API";


			GeneratedFile += "UCLASS()\n";
			GeneratedFile += "class " + Api + " U" + Name + "_K2 " + ": public UBlueprintFunctionLibrary\n";
			GeneratedFile += "{\n";
			GeneratedFile += "\tGENERATED_BODY()\n";
			GeneratedFile += "\n";
			GeneratedFile += "public:\n";

			for (const UFunction* Function : ClassToFunctions[Class])
			{
				for (const bool bAsync : TVoxelArray<bool>{ false, true })
				{
					GeneratedFile += "\t// " + Function->GetToolTipText().ToString().Replace(TEXT("\n"), TEXT("\n\t // "));
					GeneratedFile.RemoveFromEnd("\n\t // ");
					GeneratedFile.RemoveFromEnd("\n\t // ");
					GeneratedFile += "\n";

					if (bAsync)
					{
						GeneratedFile += "\t//	@param	bExecuteIfAlreadyPending	If true, this node will execute even if the last call has not yet completed. Be careful when using this on tick.\n";
					}

					{
						FString Metadata;

						if (bAsync)
						{
							Metadata = "Latent, LatentInfo = \"LatentInfo\", WorldContext = \"WorldContextObject\"";
						}


						TMap<FName, FString> MetadataMap = FVoxelUtilities::GetMetadata(Function);

						if (bAsync)
						{
							FString& AdvancedDisplay = MetadataMap.FindOrAdd("AdvancedDisplay");
							if (!AdvancedDisplay.IsEmpty())
							{
								AdvancedDisplay += ", ";
							}
							AdvancedDisplay += "bExecuteIfAlreadyPending";
						}

						for (const auto& It : MetadataMap)
						{
							if (It.Key == "Comment" ||
								It.Key == "ToolTip" ||
								It.Key == "Category" ||
								It.Key == "ModuleRelativePath" ||
								It.Key.ToString().StartsWith("CPP_Default_"))
							{
								continue;
							}

							if (!Metadata.IsEmpty())
							{
								Metadata += ", ";
							}

							Metadata += It.Key.ToString();

							if (!It.Value.IsEmpty())
							{
								Metadata += " = \"" + It.Value.ReplaceCharWithEscapedChar() + "\"";
							}
						}

						if (!Metadata.IsEmpty())
						{
							Metadata = ", meta = (" + Metadata + ")";
						}

						GeneratedFile += "\tUFUNCTION(BlueprintCallable, Category = \"" + Function->GetMetaData("Category") + "\"" + Metadata + ")\n";
					}

					GeneratedFile += "\tstatic void " + Function->GetName() + (bAsync ? "Async" : "") + "(";

					if (bAsync)
					{
						GeneratedFile += "\n\t\tUObject* WorldContextObject,";
						GeneratedFile += "\n\t\tFLatentActionInfo LatentInfo,";
					}

					for (const FProperty& Property : GetFunctionProperties(Function))
					{
						if (Property.HasAnyPropertyFlags(CPF_ReturnParm))
						{
							continue;
						}

						FString Metadata;

						TMap<FName, FString> MetadataMap;
						if (const TMap<FName, FString>* MetadataMapPtr = Property.GetMetaDataMap())
						{
							MetadataMap = *MetadataMapPtr;
						}

						for (const auto& It : MetadataMap)
						{
							if (!Metadata.IsEmpty())
							{
								Metadata += ", ";
							}

							Metadata += It.Key.ToString();

							if (!It.Value.IsEmpty())
							{
								Metadata += " = \"" + It.Value.ReplaceCharWithEscapedChar() + "\"";
							}
						}

						FString UParam;

						if (Property.HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm))
						{
							if (!UParam.IsEmpty())
							{
								UParam += ", ";
							}

							UParam += "ref";
						}

						if (Property.HasAllPropertyFlags(CPF_RequiredParm))
						{
							if (!UParam.IsEmpty())
							{
								UParam += ", ";
							}

							UParam += "Required";
						}

						if (!Metadata.IsEmpty())
						{
							if (!UParam.IsEmpty())
							{
								UParam += ", ";
							}

							UParam += "meta = (" + Metadata + ")";
						}

						if (!UParam.IsEmpty())
						{
							UParam = "UPARAM(" + UParam + ") ";
						}

						FString Default;
						if (const FString* DefaultPtr = Function->FindMetaData("CPP_Default_" + Property.GetFName()))
						{
							Default = *DefaultPtr;

							if (Property.IsA<FObjectProperty>() &&
								Default == "None")
							{
								Default = "nullptr";
							}

							if (FVoxelUtilities::IsFloat(Default))
							{
								Default = FString::SanitizeFloat(FVoxelUtilities::StringToFloat(Default));

								if (!Default.Contains("."))
								{
									Default += ".";
								}

								if (Property.IsA<FFloatProperty>())
								{
									Default += "f";
								}
							}

							if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
							{
								if (StructProperty->Struct == StaticStructFast<FVector>())
								{
									if (Default == "1.000000,0.000000,0.000000")
									{
										Default = "FVector::ForwardVector";
									}
									if (Default == "0.000000,1.000000,0.000000")
									{
										Default = "FVector::RightVector";;
									}
									if (Default == "0.000000,0.000000,1.000000")
									{
										Default = "FVector::UpVector";
									}
								}

								if (Default == "()")
								{
									Default = StructProperty->Struct->GetStructCPPName() + "()";
								}
							}
						}

						if (!Default.IsEmpty())
						{
							Default = " = " + Default;
						}

						GeneratedFile += "\n\t\t" +
							UParam +
							FVoxelUtilities::GetFunctionType(Property) +
							(Property.HasAnyPropertyFlags(CPF_OutParm) ? "& " : " ") +
							Property.GetName() + Default + ",";
					}

					GeneratedFile.RemoveFromEnd(",");

					if (bAsync)
					{
						GeneratedFile += ",\n\t\tbool bExecuteIfAlreadyPending = false";
					}

					GeneratedFile += ")\n";
					GeneratedFile += "\t{\n";

					if (bAsync)
					{
						GeneratedFile += "\t\tFVoxelLatentAction::Execute(\n";
						GeneratedFile += "\t\t\tWorldContextObject,\n";
						GeneratedFile += "\t\t\tLatentInfo,\n";
						GeneratedFile += "\t\t\tbExecuteIfAlreadyPending,\n";
						GeneratedFile += "\t\t\t[&]\n";
						GeneratedFile += "\t\t\t{\n\t\t\t\t";
					}
					else
					{
						GeneratedFile += "\t\tVoxel::ExecuteSynchronously([&]\n";
						GeneratedFile += "\t\t{\n\t\t\t";
					}

					GeneratedFile += "return U" + Function->GetOuterUClass()->GetName() + "::" + Function->GetName() + "(";

					for (const FProperty& Property : GetFunctionProperties(Function))
					{
						if (Property.HasAnyPropertyFlags(CPF_ReturnParm))
						{
							continue;
						}

						GeneratedFile += "\n\t\t\t\t" + FString(bAsync ? "\t" : "") + Property.GetName() + ",";
					}

					GeneratedFile.RemoveFromEnd(",");
					GeneratedFile += ");\n";

					if (bAsync)
					{
						GeneratedFile += "\t\t\t});\n";
					}
					else
					{
						GeneratedFile += "\t\t});\n";
					}

					GeneratedFile += "\t}\n\n";
				}

				GeneratedFile += "\n";
			}

			GeneratedFile.RemoveFromEnd("\n");
			GeneratedFile.RemoveFromEnd("\n");
			GeneratedFile.RemoveFromEnd("\n");
			GeneratedFile.RemoveFromEnd("\n");

			GeneratedFile += "\n};\n";
			GeneratedFile += "\n";
		}

		GeneratedFile.RemoveFromEnd("\n");
		GeneratedFile.RemoveFromEnd("\n");

		{
			const FString BasePath = FPaths::GetPath(FPaths::ConvertRelativePathToFull(Header));
			const FString FilePath = BasePath / FPaths::GetBaseFilename(Header) + "_K2.h";

			FString ExistingFile;
			FFileHelper::LoadFileToString(ExistingFile, *FilePath);

			// Normalize line endings
			ExistingFile.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

			if (!ExistingFile.Equals(GeneratedFile))
			{
				bModified = true;
				IFileManager::Get().Delete(*FilePath, false, true);
				ensure(FFileHelper::SaveStringToFile(GeneratedFile, *FilePath));
				LOG_VOXEL(Error, "%s written", *FilePath);
			}
		}
	}
}
#endif