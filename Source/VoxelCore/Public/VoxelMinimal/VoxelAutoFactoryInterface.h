// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"

#if WITH_EDITOR
class UFactory;
class IVoxelFactory;

class VOXELCORE_API IVoxelAutoFactoryInterface
{
public:
	IVoxelAutoFactoryInterface() = default;
	virtual ~IVoxelAutoFactoryInterface() = default;

	virtual void RegisterFactory(UClass* Class) = 0;
	virtual void RegisterBlueprintFactory(UClass* Class) = 0;
	virtual IVoxelFactory* MakeFactory(UClass* Class) = 0;

	struct FImportFactory
	{
		UClass* Class;
		FString Extension;
		FString FormatName;
		TFunction<bool(UObject*, const FString& Filename)> ImportFunction;
		TFunction<bool(UObject*)> CanReimportFunction;
		TFunction<bool(UObject*)> ReimportFunction;
	};
	virtual void RegisterImportFactory(const FImportFactory& ImportFactory) = 0;

	static IVoxelAutoFactoryInterface& GetInterface();
	static void SetInterface(IVoxelAutoFactoryInterface& NewInterface);
};

class VOXELCORE_API IVoxelFactory
{
public:
	IVoxelFactory() = default;
	virtual ~IVoxelFactory() = default;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSetupObject, UObject*);
	FOnSetupObject OnSetupObject;

	virtual UFactory* GetUFactory() = 0;
};

#define DEFINE_VOXEL_FACTORY(Class) \
	VOXEL_RUN_ON_STARTUP_GAME(Register ## Class ## Factory) \
	{ \
		IVoxelAutoFactoryInterface::GetInterface().RegisterFactory(Class::StaticClass()); \
	}

#define DEFINE_VOXEL_BLUEPRINT_FACTORY(Class) \
	VOXEL_RUN_ON_STARTUP_GAME(Register ## Class ## Factory) \
	{ \
		IVoxelAutoFactoryInterface::GetInterface().RegisterBlueprintFactory(Class::StaticClass()); \
	}

#define DEFINE_VOXEL_IMPORT_FACTORY(Class, Extension, FormatName) \
	VOXEL_RUN_ON_STARTUP_GAME(Register ## Class ## Factory) \
	{ \
		IVoxelAutoFactoryInterface::GetInterface().RegisterImportFactory( \
			{ \
				Class::StaticClass(), \
				Extension, \
				FormatName, \
				[](UObject* Object, const FString& Filename) \
				{ \
				    return CastChecked<Class>(Object)->Import(Filename); \
				}, \
				[](UObject* Object) \
				{ \
				    return Object->IsA<Class>(); \
				}, \
				[](UObject* Object) \
				{ \
				    return CastChecked<Class>(Object)->Reimport(); \
				} \
			}); \
	}
#else
#define DEFINE_VOXEL_FACTORY(Class)
#define DEFINE_VOXEL_BLUEPRINT_FACTORY(Class)
#define DEFINE_VOXEL_IMPORT_FACTORY(Class, Extension, FormatName)
#endif