// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "VoxelFactory.generated.h"

UCLASS(Abstract)
class UVoxelFactory
	: public UFactory
#if CPP
	, public IVoxelFactory
#endif
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bShouldShowInNewMenu = true;

	UVoxelFactory();

	//~ Begin UFactory Interface
	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface

	//~ Begin IVoxelFactory Interface
	virtual UFactory* GetUFactory() override { return this; }
	//~ End IVoxelFactory Interface

	void SetCreateNew(const bool bNewCreateNew)
	{
		bCreateNew = bNewCreateNew;
	}
};

UCLASS(Abstract)
class UVoxelBlueprintFactoryBase : public UVoxelFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Voxel")
	TSubclassOf<UObject> ParentClass;

public:
	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	//~ End UFactory Interface
};

UCLASS(Abstract)
class UVoxelBlueprintFactory : public UVoxelBlueprintFactoryBase
{
	GENERATED_BODY()

public:
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
};

UCLASS(Abstract)
class UVoxelBlueprintClassFactory : public UVoxelBlueprintFactoryBase
{
	GENERATED_BODY()

public:
	virtual void SetupCDO_Voxel(UObject* Object) const {}

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface
};