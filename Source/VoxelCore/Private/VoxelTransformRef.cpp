// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTransformRef.h"
#include "VoxelDependencyTracker.h"
#include "VoxelTransformRefManager.h"

FVoxelTransformRef FVoxelTransformRef::Make(const AActor& Actor)
{
	const USceneComponent* RootComponent = Actor.GetRootComponent();
	if (!ensure(RootComponent))
	{
		return {};
	}
	return Make(*RootComponent);
}

FVoxelTransformRef FVoxelTransformRef::Make(const USceneComponent& Component)
{
	const TSharedRef<FVoxelTransformRefImpl> Impl = GVoxelTransformRefManager->Make_GameThread({ FVoxelTransformRefNode(Component) });
	return FVoxelTransformRef(false, Impl);
}

void FVoxelTransformRef::NotifyTransformChanged(const USceneComponent& Component)
{
	GVoxelTransformRefManager->NotifyTransformChanged(Component);
}

bool FVoxelTransformRef::IsIdentity() const
{
	return !Impl.IsValid();
}

FMatrix FVoxelTransformRef::Get(FVoxelDependencyTracker& DependencyTracker) const
{
	if (!Impl)
	{
		return FMatrix::Identity;
	}

	// Make sure to keep the Impl alive so the dependency can be invalided
	DependencyTracker.AddDependency(Impl->Dependency);
	DependencyTracker.AddObjectToKeepAlive(Impl);

	FMatrix Transform = Impl->GetTransform();
	if (bIsInverted)
	{
		Transform = Transform.Inverse();
	}
	return Transform;
}

FMatrix FVoxelTransformRef::Get_NoDependency() const
{
	if (!Impl)
	{
		return FMatrix::Identity;
	}

	return Impl->GetTransform();
}

FVoxelTransformRef FVoxelTransformRef::Inverse() const
{
	if (!Impl)
	{
		return {};
	}

	return FVoxelTransformRef(!bIsInverted, Impl);
}

FVoxelTransformRef FVoxelTransformRef::operator*(const FVoxelTransformRef& Other) const
{
	if (!Impl)
	{
		return Other;
	}
	if (!Other.Impl)
	{
		return *this;
	}

	const TSharedPtr<FVoxelTransformRefImpl> NewImpl = Impl->Multiply_AnyThread(
		*Other.Impl,
		bIsInverted,
		Other.bIsInverted);

	return FVoxelTransformRef(false, NewImpl);
}

void FVoxelTransformRef::AddOnChanged(const FOnChanged& OnChanged, const bool bFireNow) const
{
	if (Impl)
	{
		Impl->AddOnChanged(OnChanged);
	}

	if (bFireNow)
	{
		(void)OnChanged.ExecuteIfBound(Get_NoDependency());
	}
}