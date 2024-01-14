// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMinimal.h"

#if WITH_EDITOR
class FVoxelAutoFactoryQueue : public IVoxelAutoFactoryInterface
{
public:
	FVoxelAutoFactoryQueue() = default;

	virtual void RegisterFactory(UClass* Class) override
	{
		FactoriesToRegister.Add(Class);
	}
	virtual void RegisterBlueprintFactory(UClass* Class) override
	{
		BlueprintFactoriesToRegister.Add(Class);
	}
	virtual IVoxelFactory* MakeFactory(UClass* Class) override
	{
		ensure(false);
		return nullptr;
	}

	virtual void RegisterImportFactory(const FImportFactory& ImportFactory) override
	{
		ImportFactoriesToRegister.Add(ImportFactory);
	}

	TArray<UClass*> FactoriesToRegister;
	TArray<UClass*> BlueprintFactoriesToRegister;
	TArray<FImportFactory> ImportFactoriesToRegister;
};

IVoxelAutoFactoryInterface* GVoxelAutoFactoryInterface = nullptr;
FVoxelAutoFactoryQueue* GVoxelAutoFactoryInterfaceQueue = nullptr;

IVoxelAutoFactoryInterface& IVoxelAutoFactoryInterface::GetInterface()
{
	if (!GVoxelAutoFactoryInterface)
	{
		if (!GVoxelAutoFactoryInterfaceQueue)
		{
			GVoxelAutoFactoryInterfaceQueue = new FVoxelAutoFactoryQueue();
		}
		return *GVoxelAutoFactoryInterfaceQueue;
	}
	return *GVoxelAutoFactoryInterface;
}

void IVoxelAutoFactoryInterface::SetInterface(IVoxelAutoFactoryInterface& NewInterface)
{
	check(!GVoxelAutoFactoryInterface);
	GVoxelAutoFactoryInterface = &NewInterface;

	if (GVoxelAutoFactoryInterfaceQueue)
	{
		for (UClass* Factory : GVoxelAutoFactoryInterfaceQueue->FactoriesToRegister)
		{
			GVoxelAutoFactoryInterface->RegisterFactory(Factory);
		}
		for (UClass* BlueprintFactory : GVoxelAutoFactoryInterfaceQueue->BlueprintFactoriesToRegister)
		{
			GVoxelAutoFactoryInterface->RegisterBlueprintFactory(BlueprintFactory);
		}
		for (const FImportFactory& ImportFactory : GVoxelAutoFactoryInterfaceQueue->ImportFactoriesToRegister)
		{
			GVoxelAutoFactoryInterface->RegisterImportFactory(ImportFactory);
		}
		delete GVoxelAutoFactoryInterfaceQueue;
	}
}
#endif