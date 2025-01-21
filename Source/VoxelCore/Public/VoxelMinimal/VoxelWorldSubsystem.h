// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "VoxelMinimal/Utilities/VoxelObjectUtilities.h"

class VOXELCORE_API IVoxelWorldSubsystem : public TSharedFromThis<IVoxelWorldSubsystem>
{
public:
	IVoxelWorldSubsystem() = default;
	virtual ~IVoxelWorldSubsystem() = default;

	VOXEL_COUNT_INSTANCES();

	virtual void Tick() {}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

public:
	// Always safe to call in Tick
	FORCEINLINE UWorld* GetWorld() const
	{
		checkVoxelSlow(IsInGameThread());

		UWorld* World = PrivateWorld.ResolveObjectPtr();
		ensureVoxelSlow(World);
		return World;
	}
	FORCEINLINE TObjectKey<UWorld> GetWorld_AnyThread() const
	{
		return PrivateWorld;
	}

protected:
	static TSharedRef<IVoxelWorldSubsystem> GetInternal(
		TObjectKey<UWorld> World,
		FName Name,
		TSharedRef<IVoxelWorldSubsystem>(*Constructor)());

	static TVoxelArray<TSharedRef<IVoxelWorldSubsystem>> GetAllInternal(FName Name);

private:
	TObjectKey<UWorld> PrivateWorld;
};

#define GENERATED_VOXEL_WORLD_SUBSYSTEM_BODY(Name) \
	Name() = default; \
	static TSharedRef<IVoxelWorldSubsystem> __Constructor() \
	{ \
		return MakeShared<Name>(); \
	} \
	FORCEINLINE static TSharedRef<Name> Get(const UWorld* World) \
	{ \
		return Get(MakeObjectKey(ConstCast(World))); \
	} \
	FORCEINLINE static TSharedRef<Name> Get(const TObjectKey<UWorld> World) \
	{ \
		return StaticCastSharedRef<Name>(GetInternal(World, STATIC_FNAME(#Name), &__Constructor)); \
	} \
	FORCEINLINE static TVoxelArray<TSharedRef<Name>> GetAll() \
	{ \
		return ReinterpretCastRef<TVoxelArray<TSharedRef<Name>>>(GetAllInternal(STATIC_FNAME(#Name))); \
	}