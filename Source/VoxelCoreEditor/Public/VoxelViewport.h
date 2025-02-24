// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreEditorMinimal.h"

class SVoxelEditorViewport;
class IVoxelViewportInterface;
class FVoxelViewportPreviewScene;

class VOXELCOREEDITOR_API SVoxelViewport
	: public SCompoundWidget
	, public FGCObject
{
public:
	VOXEL_SLATE_ARGS()
	{
	};

	virtual ~SVoxelViewport() override;

	void Construct(const FArguments& Args);
	void Initialize(const TSharedRef<IVoxelViewportInterface>& Interface);

public:
	FVector GetViewLocation() const;
	FRotator GetViewRotation() const;

	void SetViewLocation(const FVector& Location);
	void SetViewRotation(const FRotator& Rotation);

public:
	void SetStatsText(const FString& Text);
	void SetSkyScale(float Scale);
	void SetFloorScale(const FVector& Scale);

public:
	FORCEINLINE UWorld* GetWorld() const
	{
		return World;
	}
	FORCEINLINE USceneComponent* GetRootComponent() const
	{
		return RootComponent;
	}

	template<typename T>
	FORCEINLINE T* SpawnActor()
	{
		return CastChecked<T>(this->SpawnActor(T::StaticClass()), ECastCheckedType::NullAllowed);
	}
	template<typename T>
	FORCEINLINE T* CreateComponent()
	{
		return CastChecked<T>(this->CreateComponent(T::StaticClass()), ECastCheckedType::NullAllowed);
	}

	AActor* SpawnActor(UClass* Class);
	UActorComponent* CreateComponent(UClass* Class);

public:
	//~ Begin FGCObject Interface
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

private:
	TSharedPtr<SVoxelEditorViewport> EditorViewport;
	TSharedPtr<FVoxelViewportPreviewScene> PreviewScene;
	FText StatsText;

	TObjectPtr<UWorld> World;
	TObjectPtr<USceneComponent> RootComponent;
	TArray<TObjectPtr<AActor>> Actors;
};