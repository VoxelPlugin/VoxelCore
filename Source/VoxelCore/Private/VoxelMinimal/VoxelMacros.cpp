// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/AsyncPackageLoader.h"

FVoxelCounter32 GVoxelNumGCScopes;

struct FVoxelGCScopeGuard::FImpl
{
	FGCScopeGuard Guard;

	FImpl()
	{
		GVoxelNumGCScopes.Increment();
	}
	~FImpl()
	{
		GVoxelNumGCScopes.Decrement();
	}
};

FVoxelGCScopeGuard::FVoxelGCScopeGuard()
{
	if (IsInGameThread())
	{
		return;
	}

	VOXEL_FUNCTION_COUNTER();
	Impl = MakePimpl<FImpl>();
}

FVoxelGCScopeGuard::~FVoxelGCScopeGuard()
{
	if (!Impl)
	{
		return;
	}

	VOXEL_FUNCTION_COUNTER();
	Impl.Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DEFINE_PRIVATE_ACCESS(FUObjectArray, ObjObjectsCritical);

bool Voxel_CanAccessUObject()
{
	if (IsInGameThread() ||
		IsInParallelGameThread() ||
		IsInAsyncLoadingThread())
	{
		return true;
	}

	if (IsGarbageCollecting() ||
		GVoxelNumGCScopes.Get() > 0)
	{
		return true;
	}

	// If we fail to lock GUObjectArray it means it's likely locked already, meaning accessing objects async is safe
	// This is needed to not incorrectly raise errors when GetAllReferencersIncludingWeak is called
	if (PrivateAccess::ObjObjectsCritical(GUObjectArray).TryLock())
	{
		PrivateAccess::ObjObjectsCritical(GUObjectArray).Unlock();
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VOXELCORE_API bool GIsVoxelCoreModuleLoaded = false;
VOXELCORE_API FSimpleMulticastDelegate GOnVoxelModuleUnloaded_DoCleanup;
VOXELCORE_API FSimpleMulticastDelegate GOnVoxelModuleUnloaded;

struct FVoxelConsoleVariable
{
	TFunction<void()> OnChanged;
	TFunction<void()> Tick;
};
TVoxelArray<FVoxelConsoleVariable>& GetVoxelConsoleVariables()
{
	static TVoxelArray<FVoxelConsoleVariable> Variables;
	return Variables;
}

FVoxelConsoleVariableHelper::FVoxelConsoleVariableHelper(
	TFunction<void()> OnChanged,
	TFunction<void()> Tick)
{
	GetVoxelConsoleVariables().Add(FVoxelConsoleVariable
	{
		MoveTemp(OnChanged),
		MoveTemp(Tick)
	});
}

class FVoxelConsoleVariablesSingleton : public FVoxelSingleton
{
public:
	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		IConsoleManager::Get().RegisterConsoleVariableSink_Handle(MakeLambdaDelegate([]
		{
			VOXEL_SCOPE_COUNTER("ConsoleVariableSink VoxelConsoleVariables");
			ensure(IsInGameThread());

			for (const FVoxelConsoleVariable& Variable : GetVoxelConsoleVariables())
			{
				Variable.OnChanged();
			}
		}));
	}
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		for (const FVoxelConsoleVariable& Variable : GetVoxelConsoleVariables())
		{
			Variable.Tick();
		}
	}
	//~ End FVoxelSingleton Interface
};
FVoxelConsoleVariablesSingleton* GVoxelConsoleVariablesSingleton = new FVoxelConsoleVariablesSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelRunOnStartupStatics
{
	struct FFunctions
	{
		bool bRun = false;
		TArray<TPair<int32, TFunction<void()>>> Functions;

		void Add(const int32 Priority, const TFunction<void()>& Lambda)
		{
			check(!bRun);
			Functions.Add({ Priority, Lambda });
		}
		void Execute()
		{
			VOXEL_FUNCTION_COUNTER();

			check(!bRun);
			bRun = true;

			Functions.Sort([](const TPair<int32, TFunction<void()>>& A, const TPair<int32, TFunction<void()>>& B)
			{
				return A.Key > B.Key;
			});

			for (const auto& It : Functions)
			{
				It.Value();
			}

			Functions.Empty();
		}
	};

	FFunctions GameFunctions;
	FFunctions EditorFunctions;
	FFunctions EditorCommandletFunctions;

	static FVoxelRunOnStartupStatics& Get()
	{
		static FVoxelRunOnStartupStatics Statics;
		return Statics;
	}
};

FVoxelRunOnStartupPhaseHelper::FVoxelRunOnStartupPhaseHelper(const EVoxelRunOnStartupPhase Phase, const int32 Priority, TFunction<void()> Lambda)
{
	if (Phase == EVoxelRunOnStartupPhase::Game)
	{
		FVoxelRunOnStartupStatics::Get().GameFunctions.Add(Priority, MoveTemp(Lambda));
	}
	else if (Phase == EVoxelRunOnStartupPhase::Editor)
	{
		FVoxelRunOnStartupStatics::Get().EditorFunctions.Add(Priority, MoveTemp(Lambda));
	}
	else
	{
		check(Phase == EVoxelRunOnStartupPhase::EditorCommandlet);
		FVoxelRunOnStartupStatics::Get().EditorCommandletFunctions.Add(Priority, MoveTemp(Lambda));
	}
}

FDelayedAutoRegisterHelper GVoxelRunOnStartup_Game(EDelayedRegisterRunPhase::ObjectSystemReady, []
{
	IPluginManager::Get().OnLoadingPhaseComplete().AddLambda([](const ELoadingPhase::Type LoadingPhase, const bool bSuccess)
	{
		if (LoadingPhase != ELoadingPhase::PostDefault)
		{
			return;
		}

		FVoxelRunOnStartupStatics::Get().GameFunctions.Execute();

		if (WITH_EDITOR)
		{
			FVoxelRunOnStartupStatics::Get().EditorCommandletFunctions.Execute();
		}
	});
});

FDelayedAutoRegisterHelper GVoxelRunOnStartup_Editor(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	if (!GIsEditor)
	{
		return;
	}

	FVoxelRunOnStartupStatics::Get().EditorFunctions.Execute();
});