// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Factories/VoxelFactory.h"
#include "Factories/VoxelAssetClassParentFilter.h"
#include "KismetCompilerModule.h"
#include "Kismet2/SClassPickerDialog.h"

UVoxelFactory::UVoxelFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UVoxelFactory::ShouldShowInNewMenu() const
{
	return bShouldShowInNewMenu && Super::ShouldShowInNewMenu();
}

UObject* UVoxelFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UObject* Object = NewObject<UObject>(InParent, Class, Name, Flags);
	if (Object)
	{
		OnSetupObject.Broadcast(Object);
		Object->PostEditChange();
	}
	return Object;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UVoxelBlueprintFactoryBase::ConfigureProperties()
{
	ParentClass = nullptr;

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	const TSharedRef<FVoxelAssetClassParentFilter> Filter = MakeShared<FVoxelAssetClassParentFilter>();
	Options.ClassFilters.Add(Filter);

	Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists;
	Filter->AllowedChildrenOfClasses.Add(SupportedClass);

	const FText TitleText = INVTEXT("Pick Parent Class");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, SupportedClass);

	if (bPressedOk)
	{
		ParentClass = ChosenClass;
	}

	return bPressedOk;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* UVoxelBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (!ParentClass)
	{
		return nullptr;
	}

	UObject* Object = NewObject<UObject>(InParent, ParentClass, Name, Flags);
	if (Object)
	{
		OnSetupObject.Broadcast(Object);
		Object->PostEditChange();
	}
	return Object;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* UVoxelBlueprintClassFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (!ParentClass)
	{
		return nullptr;
	}

	UClass* BlueprintClass = nullptr;
	UClass* BlueprintGeneratedClass = nullptr;

	const IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetBlueprintTypesForClass(ParentClass, BlueprintClass, BlueprintGeneratedClass);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, BlueprintClass, BlueprintGeneratedClass, NAME_None);
	if (!ensure(Blueprint->GeneratedClass))
	{
		return nullptr;
	}

	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	SetupCDO_Voxel(CDO);
	CDO->PostEditChange();
	CDO->MarkPackageDirty();

	// Compile changes to CDO
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	return Blueprint;
}