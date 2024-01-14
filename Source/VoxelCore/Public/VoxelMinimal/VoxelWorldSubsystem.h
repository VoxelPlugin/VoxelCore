// Copyright Voxel Plugin, Inc. All Rights Reserved.

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
		checkVoxelSlow(PrivateWorld.IsValid());
		return PrivateWorld.Get();
	}
	FORCEINLINE FObjectKey GetWorld_AnyThread() const
	{
		return MakeObjectKey(PrivateWorld);
	}

protected:
	static TSharedRef<IVoxelWorldSubsystem> GetInternal(
		FObjectKey World,
		FName Name,
		TSharedRef<IVoxelWorldSubsystem>(*Constructor)());

	static TVoxelArray<TSharedRef<IVoxelWorldSubsystem>> GetAllInternal(FName Name);

private:
	TWeakObjectPtr<UWorld> PrivateWorld;
};

#define GENERATED_VOXEL_WORLD_SUBSYSTEM_BODY(Name) \
	Name() = default; \
	static TSharedRef<IVoxelWorldSubsystem> __Constructor() \
	{ \
		return MakeVoxelShared<Name>(); \
	} \
	FORCEINLINE static TSharedRef<Name> Get(const UWorld* World) \
	{ \
		return Get(FObjectKey(World)); \
	} \
	FORCEINLINE static TSharedRef<Name> Get(const FObjectKey World) \
	{ \
		return StaticCastSharedRef<Name>(GetInternal(World, STATIC_FNAME(#Name), &__Constructor)); \
	} \
	FORCEINLINE static TVoxelArray<TSharedRef<Name>> GetAll() \
	{ \
		return ReinterpretCastRef<TVoxelArray<TSharedRef<Name>>>(GetAllInternal(STATIC_FNAME(#Name))); \
	}