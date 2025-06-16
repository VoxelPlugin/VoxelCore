// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#if WITH_EDITOR
#include "VoxelHeaderGenerator.h"
#include "SourceCodeNavigation.h"
#include "StructUtils/InstancedStruct.h"
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

	{
		TVoxelMap<const UClass*, FString> ClassToHeader;
		for (const auto& It : ClassToFunctions)
		{
			const UClass* Class = It.Key;

			FString Header;
			if (!FSourceCodeNavigation::FindClassHeaderPath(Class, Header))
			{
				continue;
			}

			ClassToHeader.Add_EnsureNew(Class, Header);
		}
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

	TVoxelMap<FString, FVoxelHeaderGenerator> HeaderToFile;
	for (const auto& It : ClassToFunctions)
	{
		FString HeaderName;
		if (!FVoxelHeaderGenerator::GetHeaderName(It.Key, HeaderName))
		{
			continue;
		}

		FVoxelHeaderGenerator& File = HeaderToFile.FindOrAdd_WithDefault(HeaderName, FVoxelHeaderGenerator(HeaderName + "_K2", It.Key));
		
		File.AddInclude("VoxelLatentAction.h");
		File.AddInclude(It.Key);

		FVoxelHeaderObject& Object = File.AddClass(It.Key->GetName() + "_K2", true);
		Object.AddParent<UBlueprintFunctionLibrary>();

		for (const UFunction* Function : It.Value)
		{
			for (const bool bAsync : TVoxelArray<bool>{ false, true })
			{
				FVoxelHeaderFunction& Func = Object.AddFunction(Function->GetName() + (bAsync ? "Async" : ""));

				Func.AddComment(Function->GetToolTipText().ToString());

				if (bAsync)
				{
					Func.AddComment("@param bExecuteIfAlreadyPending	If true, this node will execute even if the last call has not yet completed. Be careful when using this on tick.");
				}

				Func.AddMetadata(false, "BlueprintCallable");
				Func.AddMetadata(false, "Category", Function->GetMetaData("Category"));

				for (const auto& MetadataIt : FVoxelUtilities::GetMetadata(Function))
				{
					if (MetadataIt.Key == "Comment" ||
						MetadataIt.Key == "ToolTip" ||
						MetadataIt.Key == "Category" ||
						MetadataIt.Key == "ModuleRelativePath" ||
						MetadataIt.Key.ToString().StartsWith("CPP_Default_"))
					{
						continue;
					}

					Func.AddMetadata(true, MetadataIt.Key.ToString(), MetadataIt.Value);
				}

				if (bAsync)
				{
					Func.AddMetadata(true, "Latent");
					Func.AddMetadata(true, "LatentInfo", "LatentInfo");
					Func.AddMetadata(true, "WorldContext", "WorldContextObject");
					Func.AddMetadata(true, "AdvancedDisplay", "bExecuteIfAlreadyPending");
				}

				if (bAsync)
				{
					Func.AddArgument<UObject*>("WorldContextObject");
					Func.AddArgument<FLatentActionInfo>("LatentInfo");
				}

				for (const FProperty& Property : GetFunctionProperties(Function))
				{
					if (Property.HasAnyPropertyFlags(CPF_ReturnParm))
					{
						continue;
					}

					FVoxelHeaderFunctionArgument& Argument = Func.AddArgument(Property);

					TMap<FName, FString> MetadataMap;
					if (const TMap<FName, FString>* MetadataMapPtr = Property.GetMetaDataMap())
					{
						MetadataMap = *MetadataMapPtr;
					}

					for (const auto& MetadataIt : MetadataMap)
					{
						if (MetadataIt.Key == "NativeConst")
						{
							Argument.Const();
							continue;
						}

						if (MetadataIt.Key == "BaseStruct" &&
							Property.IsA<FStructProperty>() &&
							CastFieldChecked<FStructProperty>(Property).Struct == FInstancedStruct::StaticStruct())
						{
							continue;
						}

						Argument.AddMetadata(true, MetadataIt.Key.ToString(), MetadataIt.Value);
					}

					if (Property.HasAnyPropertyFlags(CPF_OutParm))
					{
						Argument.Ref();
					}

					if (Property.HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm) &&
						!Property.HasAnyPropertyFlags(CPF_ConstParm))
					{
						Argument.AddMetadata(false, "ref");
					}

					if (Property.HasAllPropertyFlags(CPF_RequiredParm))
					{
						Argument.AddMetadata(false, "Required");
					}

					if (const FString* DefaultPtr = Function->FindMetaData("CPP_Default_" + Property.GetFName()))
					{
						FString Default = *DefaultPtr;
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

						if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
						{
							Default = EnumProperty->GetCPPType(nullptr, 0) + "::" + Default;
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
								if (Default == "0.000000,0.000000,0.000000")
								{
									Default = "FVector::ZeroVector";
								}
							}

							if (StructProperty->Struct == StaticStructFast<FRotator>())
							{
								if (Default.IsEmpty())
								{
									Default = "FRotator::ZeroRotator";
								}
							}

							if (Default == "()")
							{
								Default = StructProperty->Struct->GetStructCPPName() + "()";
							}
						}

						if (ensure(!Default.IsEmpty()))
						{
							Argument.SetDefault(Default);
						}
					}
				}

				if (bAsync)
				{
					Func.AddArgument<bool>("bExecuteIfAlreadyPending").SetDefault("false");
				}

				if (bAsync)
				{
					Func += "FVoxelLatentAction::Execute(";
					Func += "	WorldContextObject,";
					Func += "	LatentInfo,";
					Func += "	bExecuteIfAlreadyPending,";
					Func += "	[&]";
					Func += "	{";

					Func++;
					Func++;
				}
				else
				{
					Func += "Voxel::ExecuteSynchronously([&]";
					Func += "{";
					Func++;
				}

				Func += "return U" + Function->GetOuterUClass()->GetName() + "::" + Function->GetName() + "(";
				Func++;

				FString CallArguments;
				for (const FProperty& Property : GetFunctionProperties(Function))
				{
					if (Property.HasAnyPropertyFlags(CPF_ReturnParm))
					{
						continue;
					}

					CallArguments += Property.GetName() + ",\n";
				}
				CallArguments.RemoveFromEnd(",\n");

				Func += CallArguments + ");";
				Func--;
				Func--;

				Func += "});";
			}
		}
	}

	for (const auto& It : HeaderToFile)
	{
		if (It.Value.CreateFile())
		{
			bModified = true;
		}
	}
}
#endif