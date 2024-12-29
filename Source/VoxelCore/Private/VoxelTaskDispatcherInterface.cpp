// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelTaskDispatcherInterface.h"
#include "VoxelGlobalTaskDispatcher.h"

DEFINE_VOXEL_INSTANCE_COUNTER(IVoxelTaskDispatcher);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelTaskDispatcherManager
{
public:
	FVoxelCriticalSection CriticalSection;
	TVoxelSparseArray<TWeakPtr<IVoxelTaskDispatcher>> TaskDispatchers_RequiresLock;
	FVoxelCounter32 SerialCounter;
};
FVoxelTaskDispatcherManager* GVoxelTaskDispatcherManager = new FVoxelTaskDispatcherManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelTaskDispatcherRef::FVoxelTaskDispatcherRef(const IVoxelTaskDispatcher& Dispatcher)
{
	*this = Dispatcher.SelfRef.Get();

	if (!IsNull())
	{
		return;
	}

	VOXEL_SCOPE_LOCK(GVoxelTaskDispatcherManager->CriticalSection);

	*this = Dispatcher.SelfRef.Get();

	if (!IsNull())
	{
		return;
	}

	Index = GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.Add(ConstCast(Dispatcher).AsWeak());
	Serial = GVoxelTaskDispatcherManager->SerialCounter.Increment_ReturnNew();

	ConstCast(Dispatcher.SelfRef).Set(*this);
}

TSharedPtr<IVoxelTaskDispatcher> FVoxelTaskDispatcherRef::Pin() const
{
	if (IsNull())
	{
		return nullptr;
	}

	VOXEL_SCOPE_LOCK(GVoxelTaskDispatcherManager->CriticalSection);

	if (!GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.IsValidIndex(Index))
	{
		return nullptr;
	}

	TSharedPtr<IVoxelTaskDispatcher> Dispatcher = GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock[Index].Pin();
	if (!Dispatcher)
	{
		return nullptr;
	}

	if (Dispatcher->SelfRef.Get().Serial != Serial)
	{
		return nullptr;
	}

	return Dispatcher;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IVoxelTaskDispatcher::~IVoxelTaskDispatcher()
{
	const FVoxelTaskDispatcherRef Ref = SelfRef.Get();
	if (Ref.IsNull())
	{
		return;
	}

	VOXEL_SCOPE_LOCK(GVoxelTaskDispatcherManager->CriticalSection);

	check(GetWeakPtrObject_Unsafe(GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock[Ref.Index]) == this);
	GVoxelTaskDispatcherManager->TaskDispatchers_RequiresLock.RemoveAt(Ref.Index);
}

void IVoxelTaskDispatcher::DumpPromises()
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	TVoxelMap<FVoxelStackFrames, int32> StackFramesToCount;
	StackFramesToCount.Reserve(StackFrames_RequiresLock.Num());

	for (const FVoxelStackFrames& StackFrames : StackFrames_RequiresLock)
	{
		StackFramesToCount.FindOrAdd(StackFrames)++;
	}

	StackFramesToCount.ValueSort([](const int32 A, const int32 B)
	{
		return A > B;
	});

	for (const auto& It : StackFramesToCount)
	{
		LOG_VOXEL(Log, "x%d:", It.Value);

		for (const FString& Line : FVoxelUtilities::StackFramesToString(It.Key))
		{
			LOG_VOXEL(Log, "\t%s", *Line);
		}
	}
}

void IVoxelTaskDispatcher::Dispatch(
	const EVoxelFutureThread Thread,
	TVoxelUniqueFunction<void()> Lambda)
{
#if VOXEL_DEBUG
	Lambda = [this, Lambda = MoveTemp(Lambda)]
	{
		check(&FVoxelTaskDispatcherScope::Get() == this);
		Lambda();
	};
#endif

	DispatchImpl(Thread, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const uint32 GVoxelTaskDispatcherScopeTLS = FPlatformTLS::AllocTlsSlot();