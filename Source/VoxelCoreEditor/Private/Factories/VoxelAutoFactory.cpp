// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Factories/VoxelAutoFactory.h"

void UVoxelAutoFactory::PostInitProperties()
{
	Super::PostInitProperties();

	// Manually copy edited data from the CDO
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		bCreateNew = GetClass()->GetDefaultObject<UVoxelAutoFactory>()->bCreateNew;
		SupportedClass = GetClass()->GetDefaultObject<UVoxelAutoFactory>()->SupportedClass;
	}
}

void UVoxelAutoBlueprintFactory::PostInitProperties()
{
	Super::PostInitProperties();

	// Manually copy edited data from the CDO
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		bCreateNew = GetClass()->GetDefaultObject<UVoxelAutoBlueprintFactory>()->bCreateNew;
		SupportedClass = GetClass()->GetDefaultObject<UVoxelAutoBlueprintFactory>()->SupportedClass;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelAutoImportFactory::UVoxelAutoImportFactory()
{
	bEditorImport = true;
}

void UVoxelAutoImportFactory::PostInitProperties()
{
	Super::PostInitProperties();

	// Manually copy edited data from the CDO
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SupportedClass = GetClass()->GetDefaultObject<UVoxelAutoImportFactory>()->SupportedClass;
		Formats = GetClass()->GetDefaultObject<UVoxelAutoImportFactory>()->Formats;
		Extension = GetClass()->GetDefaultObject<UVoxelAutoImportFactory>()->Extension;
		ImportFunction = GetClass()->GetDefaultObject<UVoxelAutoImportFactory>()->ImportFunction;
		ReimportFunction = GetClass()->GetDefaultObject<UVoxelAutoImportFactory>()->ReimportFunction;
	}
}

bool UVoxelAutoImportFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename) == Extension;
}

UObject* UVoxelAutoImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, const FName InName, const EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	ensure(Flags & RF_Transactional);

	UObject* Object = NewObject<UObject>(InParent, InClass, InName, Flags);
	if (!ensure(ImportFunction) || !ImportFunction(Object, Filename))
	{
		return nullptr;
	}

	return Object;
}

bool UVoxelAutoImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return CanReimportFunction && CanReimportFunction(Obj);
}

void UVoxelAutoImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
}

EReimportResult::Type UVoxelAutoImportFactory::Reimport(UObject* Obj)
{
	if (!ReimportFunction || !ReimportFunction(Obj))
	{
		return EReimportResult::Failed;
	}

	return EReimportResult::Succeeded;
}

int32 UVoxelAutoImportFactory::GetPriority() const
{
	return ImportPriority;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelAutoFactoryImpl::RegisterFactory(UClass* Class)
{
	RegisterFactoryImpl(Class, FactoryCounter++, "VoxelFactoryDummy");
}

void FVoxelAutoFactoryImpl::RegisterBlueprintFactory(UClass* Class)
{
	RegisterFactoryImpl(Class, BlueprintFactoryCounter++, "VoxelBlueprintFactoryDummy");
}

IVoxelFactory* FVoxelAutoFactoryImpl::MakeFactory(UClass* Class)
{
	const TSubclassOf<UVoxelFactory> FactoryClass = Factories.FindRef(Class);
	if (!FactoryClass)
	{
		return nullptr;
	}

	return NewObject<UVoxelFactory>(GetTransientPackage(), FactoryClass);
}

void FVoxelAutoFactoryImpl::RegisterImportFactory(const FImportFactory& ImportFactory)
{
	const FString ClassName = "/Script/VoxelCoreEditor.VoxelAutoImportFactoryDummy" + FString::FromInt(ImportFactoryCounter++);
	const TSubclassOf<UVoxelAutoImportFactory> FactoryClass = FindObject<UClass>(nullptr, *ClassName);
	if (!ensure(FactoryClass))
	{
		return;
	}

	FactoryClass->SetMetaData("DisplayName", *(ImportFactory.Class->GetDisplayNameText().ToString() + "Factory"));

	UVoxelAutoImportFactory* Factory = FactoryClass.GetDefaultObject();
	Factory->SupportedClass = ImportFactory.Class;
	Factory->Extension = ImportFactory.Extension;
	Factory->Formats.Add(ImportFactory.Extension + ";" + ImportFactory.FormatName);
	Factory->ImportFunction = ImportFactory.ImportFunction;
	Factory->CanReimportFunction = ImportFactory.CanReimportFunction;
	Factory->ReimportFunction = ImportFactory.ReimportFunction;
}

void FVoxelAutoFactoryImpl::RegisterFactoryImpl(UClass* Class, const int32 Index, const FString& Prefix)
{
	const FString ClassName = "/Script/VoxelCoreEditor." + Prefix + FString::FromInt(Index);
	const TSubclassOf<UVoxelFactory> FactoryClass = FindObject<UClass>(nullptr, *ClassName);

	if (!ensureMsgf(FactoryClass, TEXT("Need to add more %s"), *Prefix))
	{
		return;
	}

	bool bHasOtherFactory = false;
	for (const TSubclassOf<UFactory> OtherFactoryClass : GetDerivedClasses<UFactory>())
	{
		UFactory* OtherFactory = OtherFactoryClass.GetDefaultObject();
		if (!OtherFactory->ShouldShowInNewMenu() ||
			!OtherFactory->DoesSupportClass(Class))
		{
			continue;
		}

		bHasOtherFactory = true;
	}

	FactoryClass->SetMetaData("DisplayName", *(Class->GetDisplayNameText().ToString() + "Factory"));

	UVoxelFactory* Factory = FactoryClass.GetDefaultObject();
	Factory->SupportedClass = Class;
	Factory->bShouldShowInNewMenu = !bHasOtherFactory;
	Factory->SetCreateNew(true);

	ensure(!Factories.Contains(Class));
	Factories.Add(Class, FactoryClass);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXEL_RUN_ON_STARTUP_EDITOR()
{
	// Disable all dummies until they're used

	for (const TSubclassOf<UVoxelFactory> Class : GetDerivedClasses<UVoxelFactory>())
	{
		if (Class->GetName().Contains("Dummy"))
		{
			Class.GetDefaultObject()->SetCreateNew(false);
		}
	}

	IVoxelAutoFactoryInterface::SetInterface(*new FVoxelAutoFactoryImpl());
}