// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "HAL/PlatformStackWalk.h"

UE_TRACE_CHANNEL_DEFINE(VoxelChannel);
LLM_DEFINE_TAG(Voxel, "Voxel", NAME_None, GET_STATFNAME(STAT_VoxelLLM));
DEFINE_STAT(STAT_VoxelLLM);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
// Enable by default using zero-initialization
// This way any global calling new will register LLM scopes just in case
bool GVoxelLLMDisabled = false;

thread_local int32 GVoxelLLMScopeCounter = 0;

struct FVoxelLLMScope
{
	const FLLMScope LLMScope = FLLMScope(LLMTagDeclaration_Voxel.GetUniqueName(), false, ELLMTagSet::None, ELLMTracker::Default);
#if UE_MEMORY_TAGS_TRACE_ENABLED && UE_TRACE_ENABLED
	const FMemScope MemScope = FMemScope(LLMTagDeclaration_Voxel.GetUniqueName());
#endif
};
thread_local TOptional<FVoxelLLMScope> GVoxelLLMScope;

void EnterVoxelLLMScope()
{
	ensureVoxelSlow(!GVoxelLLMDisabled);
	GVoxelLLMScopeCounter++;
	ensureVoxelSlow(GVoxelLLMScopeCounter >= 1);

	if (GVoxelLLMScopeCounter == 1)
	{
		ensureVoxelSlow(!GVoxelLLMScope);
		GVoxelLLMScope.Emplace();
	}
}
void ExitVoxelLLMScope()
{
	ensureVoxelSlow(!GVoxelLLMDisabled);
	GVoxelLLMScopeCounter--;
	ensureVoxelSlow(GVoxelLLMScopeCounter >= 0);

	if (GVoxelLLMScopeCounter == 0)
	{
		ensureVoxelSlow(GVoxelLLMScope);
		GVoxelLLMScope.Reset();
	}
}
void CheckVoxelLLMScope()
{
	if (!GIsEditor &&
		!GVoxelLLMDisabled)
	{
		ensureMsgf(GVoxelLLMScopeCounter > 0, TEXT("Missing VOXEL_FUNCTION_COUNTER or VOXEL_SCOPE_COUNTER"));
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString VoxelStats_CleanupFunctionName(const FString& FunctionName)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_VoxelStats_CleanupFunctionName);

#if PLATFORM_WINDOWS
	TArray<FString> Array;
	FunctionName.ParseIntoArray(Array, TEXT("::"));

	if (Array.Num() > 0 && Array[0] == "Voxel")
	{
		// Remove the Voxel:: prefix in namespace functions
		Array.RemoveAt(0);
	}

	// Remove lambdas
	for (int32 Index = 0; Index < Array.Num(); Index++)
	{
		if (Array[Index].StartsWith(TEXT("<lambda")) &&
			ensure(Index + 1 < Array.Num()) &&
			ensure(Array[Index + 1] == TEXT("operator ()") || Array[Index + 1] == TEXT("()")))
		{
			Array.RemoveAt(Index, 2);
			Index--;
		}
	}

	const FString FunctionNameWithoutLambda = FString::Join(Array, TEXT("::"));

	FString CleanFunctionName;
	if (FunctionNameWithoutLambda.EndsWith("operator <<"))
	{
		CleanFunctionName = FunctionNameWithoutLambda;
	}
	else
	{
		// Tricky case: TVoxelClusteredWriterBase<struct `public: static void __cdecl FVoxelSurfaceEditToolsImpl::EditVoxelValues()'::`2'::FStorage>
		int32 TemplateDepth = 0;
		for (const TCHAR& Char : FunctionNameWithoutLambda)
		{
			if (Char == TEXT('<'))
			{
				TemplateDepth++;
				continue;
			}
			if (Char == TEXT('>'))
			{
				TemplateDepth--;
				ensure(TemplateDepth >= 0);
				continue;
			}

			if (TemplateDepth == 0)
			{
				CleanFunctionName.AppendChar(Char);
			}
		}
		ensure(TemplateDepth == 0);
	}

	return CleanFunctionName;
#else
	return FunctionName;
#endif
}

FName VoxelStats_PrintfImpl(const TCHAR* Format, ...)
{
	constexpr int32 BufferSize = 512;

	TCHAR Buffer[BufferSize];
	int32 Result;
	{
		va_list VAList;
		va_start(VAList, Format);
		Result = FCString::GetVarArgs(Buffer, BufferSize, Format, VAList);
		if (!ensure(Result < BufferSize))
		{
			Result = BufferSize - 1;
		}
		va_end(VAList);
	}
	Buffer[Result] = TEXT('\0');

	return FName(FStringView(Buffer, Result));
}

FName VoxelStats_AddNum(const FString& Format, const int32 Num)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> String;
	String.Append(Format);
	String.Append(TEXTVIEW(" Num="));
	FVoxelUtilities::AppendNumber(String, Num);
	return FName(String);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
FName Voxel_GetDynamicStatName(const FName Name)
{
	static FVoxelCriticalSection CriticalSection;
	static TMap<FName, FName> Map;

	VOXEL_SCOPE_LOCK(CriticalSection);

	if (const FName* StatName = Map.Find(Name))
	{
		return *StatName;
	}

	using Group = FStatGroup_STATGROUP_VoxelMemory;

	FStartupMessages::Get().AddMetadata(
		Name,
		*Name.ToString(),
		Group::GetGroupName(),
		Group::GetGroupCategory(),
		Group::GetDescription(),
		false,
		EStatDataType::ST_int64,
		false,
		Group::GetSortByName(),
		FPlatformMemory::MCR_Physical);

	const FName StatName =
		IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(
			Name,
			Group::GetGroupName(),
			Group::GetGroupCategory(),
			true,
			false,
			EStatDataType::ST_int64,
			*Name.ToString(),
			false,
			Group::GetSortByName(),
			FPlatformMemory::MCR_Physical).GetName();

	return Map.Add(Name, StatName);
}

void Voxel_AddAmountToDynamicStat(const FName Name, const int64 Amount)
{
	VOXEL_FUNCTION_COUNTER();

	if (Amount == 0)
	{
		return;
	}

	const FName StatName = Voxel_GetDynamicStatName(Name);

	if (Amount > 0)
	{
		FThreadStats::AddMessage(StatName, EStatOperation::Add, Amount);
	}
	else
	{
		FThreadStats::AddMessage(StatName, EStatOperation::Subtract, -Amount);
	}

	TRACE_STAT_ADD(Name, Amount);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelStackTrace::Capture()
{
	StackFrames.SetNum(StackFrames.Max());
	const int64 NumStackFrames = FPlatformStackWalk::CaptureStackBackTrace(
		ReinterpretCastPtr<uint64>(StackFrames.GetData()),
		StackFrames.Num());
	StackFrames.SetNum(NumStackFrames);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if STATS
TMap<FName, const FVoxelCounter64*> GVoxelStatNameToInstanceCounter;

VOXELCORE_API void RegisterVoxelInstanceCounter(const FName StatName, const FVoxelCounter64& Counter)
{
	check(IsInGameThread());
	check(!GVoxelStatNameToInstanceCounter.Contains(StatName));
	GVoxelStatNameToInstanceCounter.Add(StatName, &Counter);
}

class FVoxelInstanceCounterTicker : public FVoxelTicker
{
public:
	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		VOXEL_FUNCTION_COUNTER();

		for (const auto& It : GVoxelStatNameToInstanceCounter)
		{
			FThreadStats::AddMessage(It.Key, EStatOperation::Set, It.Value->Get());
		}
	}
	//~ End FVoxelTicker Interface
};

VOXEL_RUN_ON_STARTUP_GAME()
{
	new FVoxelInstanceCounterTicker();

	GOnVoxelModuleUnloaded.AddLambda([]
	{
		TMap<FString, int32> Leaks;
		for (const auto& It : GVoxelStatNameToInstanceCounter)
		{
			if (It.Value->Get() == 0)
			{
				continue;
			}

			FString Name = It.Key.ToString();
			ensure(Name.RemoveFromStart("//STATGROUP_VoxelCounters//STAT_Num"));

			const int32 Index = Name.Find("///");
			if (ensure(Index != -1))
			{
				Name = Name.Left(Index);
			}

			Leaks.Add(Name, It.Value->Get());
		}

		if (Leaks.Num() == 0)
		{
			return;
		}

		FString Error = "Leaks detected:";
		for (const auto& It : Leaks)
		{
			Error += FString::Printf(TEXT("\n%s: %d instances"), *It.Key, It.Value);
		}
		ensureMsgf(false, TEXT("%s"), *Error);
	});
}
#endif