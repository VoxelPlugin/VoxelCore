// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "Factories/VoxelFactory.h"
#include "VoxelAutoFactory.generated.h"

UCLASS(Abstract)
class UVoxelAutoFactory : public UVoxelFactory
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class UVoxelAutoBlueprintFactory : public UVoxelBlueprintFactory
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class UVoxelAutoImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Extension;

	TFunction<bool(UObject*, const FString& Filename)> ImportFunction;
	TFunction<bool(UObject*)> CanReimportFunction;
	TFunction<bool(UObject*)> ReimportFunction;

	UVoxelAutoImportFactory();

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	// Begin UFactory Interface
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	// End of UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS() class UVoxelFactoryDummy0 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy1 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy2 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy3 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy4 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy5 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy6 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy7 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy8 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy9 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy10 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy11 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy12 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy13 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy14 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy15 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy16 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy17 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy18 : public UVoxelAutoFactory { GENERATED_BODY() };
UCLASS() class UVoxelFactoryDummy19 : public UVoxelAutoFactory { GENERATED_BODY() };

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS() class UVoxelBlueprintFactoryDummy0 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy1 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy2 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy3 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy4 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy5 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy6 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy7 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy8 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };
UCLASS() class UVoxelBlueprintFactoryDummy9 : public UVoxelAutoBlueprintFactory { GENERATED_BODY() };

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UCLASS() class UVoxelAutoImportFactoryDummy0 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy1 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy2 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy3 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy4 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy5 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy6 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy7 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy8 : public UVoxelAutoImportFactory { GENERATED_BODY() };
UCLASS() class UVoxelAutoImportFactoryDummy9 : public UVoxelAutoImportFactory { GENERATED_BODY() };

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelAutoFactoryImpl : public IVoxelAutoFactoryInterface
{
public:
	FVoxelAutoFactoryImpl() = default;

	virtual void RegisterFactory(UClass* Class) override;
	virtual void RegisterBlueprintFactory(UClass* Class) override;
	virtual IVoxelFactory* MakeFactory(UClass* Class) override;

	virtual void RegisterImportFactory(const FImportFactory& ImportFactory) override;

private:
	int32 FactoryCounter = 0;
	int32 ImportFactoryCounter = 0;
	int32 BlueprintFactoryCounter = 0;

	TMap<UClass*, TSubclassOf<UVoxelFactory>> Factories;

	void RegisterFactoryImpl(UClass* Class, int32 Index, const FString& Prefix);
};