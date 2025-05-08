// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMinimal.h"
#include "VoxelZipReader.h"
#include "VoxelPluginVersion.h"

#include "Misc/Fork.h"
#include "HAL/PlatformStackWalk.h"
#include "UObject/CoreRedirects.h"
#include "Interfaces/IPluginManager.h"
#include "Application/ThrottleManager.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "EditorViewportClient.h"
#endif

VOXEL_CONSOLE_COMMAND(
	"voxel.SetNumWorkerThreads",
	"Set the number of Unreal worker threads")
{
	if (Args.Num() != 1)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Usage: voxel.SetNumWorkerThreads {number}"));
		return;
	}

	if (!FVoxelUtilities::IsInt(Args[0]))
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("%s is not an integer"), *Args[0]);
		return;
	}

	FVoxelUtilities::SetNumWorkerThreads(FVoxelUtilities::StringToInt(Args[0]));
}

void FVoxelUtilities::SetNumWorkerThreads(const int32 NumWorkerThreads)
{
	VOXEL_FUNCTION_COUNTER();
	LOG_VOXEL(Log, "FVoxelUtilities::SetNumWorkerThreads %d", NumWorkerThreads);
	LOG_VOXEL(Log, "!!! Changing the number of Unreal worker threads !!!");

	const int32 NumBackgroundWorkers = FMath::Max(1, NumWorkerThreads - FMath::Min<int32>(2, NumWorkerThreads));
	const int32 NumForegroundWorkers = FMath::Max(1, NumWorkerThreads - NumBackgroundWorkers);

	LOG_VOXEL(Log, "%d background workers", NumBackgroundWorkers);
	LOG_VOXEL(Log, "%d foreground workers", NumForegroundWorkers);

	LowLevelTasks::FScheduler::Get().RestartWorkers(
		NumForegroundWorkers,
		NumBackgroundWorkers,
		FForkProcessHelper::IsForkedMultithreadInstance() ? FThread::Forkable : FThread::NonForkable,
		FPlatformAffinity::GetTaskThreadPriority(),
		FPlatformAffinity::GetTaskBPThreadPriority());
}

DEFINE_PRIVATE_ACCESS(LowLevelTasks::FScheduler, WaitingQueue);
DEFINE_PRIVATE_ACCESS(LowLevelTasks::Private::FWaitingQueue, ThreadCount);

int32 FVoxelUtilities::GetNumBackgroundWorkerThreads()
{
	const LowLevelTasks::Private::FWaitingQueue& BackgroundQueue = PrivateAccess::WaitingQueue(LowLevelTasks::FScheduler::Get())[1];
	return PrivateAccess::ThreadCount(BackgroundQueue);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelUtilities::Yield()
{
	if (IsInGameThread())
	{
		FPlatformProcess::Yield();
	}
	else
	{
		VOXEL_SCOPE_COUNTER("Sleep");
		FPlatformProcess::Sleep(0.001f);
	}
}

void FVoxelUtilities::WaitFor(const TFunctionRef<bool()> Condition)
{
	VOXEL_FUNCTION_COUNTER();

	const double StartTime = FPlatformTime::Seconds();
	double LastLogTime = -1;

	const auto CheckStalled = [&]
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartTime < 10)
		{
			return;
		}
		ensureVoxelSlow(false);

		if (Now - LastLogTime < 1)
		{
			return;
		}

		LastLogTime = Now;

		LOG_VOXEL(Warning, "FVoxelUtilities::WaitFor stuck for %fs. Callstack:\n%s",
			Now - StartTime,
			*GetPrettyCallstack_WithStats())
	};

	if (IsInGameThread())
	{
		VOXEL_SCOPE_COUNTER("Busy wait");

		while (!Condition())
		{
			CheckStalled();
			FPlatformProcess::Yield();
		}
	}
	else
	{
		{
			VOXEL_SCOPE_COUNTER("Busy wait");

			while (!Condition())
			{
				if (FPlatformTime::Seconds() - StartTime > 0.0005)
				{
					// Wait up to 500us
					break;
				}

				FPlatformProcess::Yield();
			}
		}

		while (!Condition())
		{
			CheckStalled();

			VOXEL_SCOPE_COUNTER("Sleep");
			FPlatformProcess::Sleep(0.001f);
		}
	}
}

void FVoxelUtilities::DelayedCall(TFunction<void()> Call, const float Delay)
{
	// Delay will be inaccurate if not on game thread but that's fine
	Voxel::GameTask([=]
	{
		FTSTicker::GetCoreTicker().AddTicker(MakeLambdaDelegate([=](float)
		{
			VOXEL_FUNCTION_COUNTER();
			Call();
			return false;
		}), Delay);
	});
}

FString FVoxelUtilities::Unzip(const TArray<uint8>& Data, TMap<FString, TVoxelArray64<uint8>>& OutFiles)
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FVoxelZipReader> ZipReader = FVoxelZipReader::Create(Data);
	if (!ZipReader)
	{
		return "Failed to unzip";
	}

	for (const FString& File : ZipReader->GetFiles())
	{
		TVoxelArray64<uint8> FileData;
		if (!ZipReader->TryLoad(File, FileData))
		{
			return "Failed to unzip " + File;
		}

		ensure(!OutFiles.Contains(File));
		OutFiles.Add(File, MoveTemp(FileData));
	}

	return {};
}

#if WITH_EDITOR
void FVoxelUtilities::EnsureViewportIsUpToDate()
{
	VOXEL_FUNCTION_COUNTER();

	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		// No need to do anything, slate is not throttling
		return;
	}

	const FViewport* Viewport = GEditor->GetActiveViewport();
	if (!Viewport)
	{
		return;
	}

	const FViewportClient* Client = Viewport->GetClient();
	if (!Client)
	{
		return;
	}

	for (FEditorViewportClient* EditorViewportClient : GEditor->GetAllViewportClients())
	{
		EditorViewportClient->Invalidate(false, false);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IPlugin& FVoxelUtilities::GetPlugin()
{
	static TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Voxel");
	if (!Plugin)
	{
		Plugin = IPluginManager::Get().FindPlugin("VoxelCore");
	}
	return *Plugin;
}

FVoxelPluginVersion FVoxelUtilities::GetPluginVersion()
{
	FString VersionName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-PluginVersionName="), VersionName))
	{
		VersionName = GetPlugin().GetDescriptor().VersionName;
	}

	if (VersionName == "Unknown")
	{
		return {};
	}

	FVoxelPluginVersion Version;
	ensure(Version.Parse(VersionName));

	return Version;
}

bool FVoxelUtilities::IsDevWorkflow()
{
	if (VOXEL_DEV_WORKFLOW)
	{
		static const bool bValue = !FParse::Param(FCommandLine::Get(), TEXT("NoVoxelDevWorkflow"));
		return bValue;
	}
	else
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("VoxelDevWorkflow"));
		return bValue;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void FVoxelUtilities::CleanupRedirects(const FString& RedirectsPath)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(GIsEditor))
	{
		// Editor objects won't be loaded, so all editor redirects would be removed
		return;
	}

	TVoxelArray<FString> Lines;
	if (!ensure(FFileHelper::LoadFileToStringArray(Lines, *RedirectsPath)))
	{
		return;
	}

	TVoxelArray<FString> NewLines;
	NewLines.Reserve(Lines.Num());

	for (const FString& OriginalLine : Lines)
	{
		if (OriginalLine.StartsWith("[CoreRedirects]") ||
			OriginalLine.StartsWith(";") ||
			OriginalLine.TrimStartAndEnd().IsEmpty())
		{
			NewLines.Add(OriginalLine);
			continue;
		}

		FString Line = OriginalLine;
		if (!ensure(Line.RemoveFromStart("+")))
		{
			return;
		}

		int32 Index = 0;

		const auto IsValidIndex = [&]
		{
			return Index < Line.Len();
		};

		const auto SkipWhitespaces = [&]
		{
			while (
				Index < Line.Len() &&
				FChar::IsWhitespace(Line[Index]))
			{
				Index++;
			}
		};

		const auto Next = [&]
		{
			SkipWhitespaces();

			if (!ensure(Index < Line.Len()))
			{
				return TEXT('\0');
			}

			return Line[Index++];
		};

		const auto Parse = [&](const TCHAR Delimiter)
		{
			FString Result;
			while (true)
			{
				if (Index == Line.Len())
				{
					ensure(false);
					return Result;
				}

				if (FChar::IsWhitespace(Line[Index]))
				{
					Index++;
					continue;
				}

				if (Line[Index] == Delimiter)
				{
					Index++;
					break;
				}

				Result += Line[Index];
				Index++;
			}
			return Result;
		};

		const FString Type = Parse(TEXT('='));

		if (!ensure(Next() == TEXT('(')))
		{
			return;
		}

		TVoxelMap<FString, FString> KeyToValue;

		bool bSkip = false;
		while (true)
		{
			const FString Key = Parse(TEXT('='));

			if (Key == "ValueChanges")
			{
				// TODO?
				NewLines.Add(OriginalLine);
				bSkip = true;
				break;
			}

			if (!ensure(Next() == TEXT('"')))
			{
				return;
			}

			const FString Value = Parse(TEXT('"'));

			if (!ensure(!KeyToValue.Contains(Key)))
			{
				return;
			}
			KeyToValue.Add_EnsureNew(Key, Value);

			SkipWhitespaces();

			if (!ensure(IsValidIndex()))
			{
				return;
			}

			if (Line[Index] == TEXT(','))
			{
				Index++;
				continue;
			}

			if (Line[Index] == TEXT(')'))
			{
				Index++;
				break;
			}
		}

		if (bSkip)
		{
			continue;
		}

		SkipWhitespaces();

		if (!ensure(Index == Line.Len()))
		{
			return;
		}

		if (!ensure(KeyToValue.KeyArray() == TVoxelArray<FString>({ "OldName", "NewName" })))
		{
			return;
		}

		const FString OldName = KeyToValue["OldName"];
		FString NewName = KeyToValue["NewName"];

		const auto ApplyRedirect = [&](const ECoreRedirectFlags Flags)
		{
			FCoreRedirectObjectName RedirectedName;
			if (FCoreRedirects::RedirectNameAndValues(
				Flags,
				FCoreRedirectObjectName(NewName),
				RedirectedName,
				nullptr,
				ECoreRedirectMatchFlags::AllowPartialMatch))
			{
				NewName = FTopLevelAssetPath(RedirectedName.PackageName, RedirectedName.ObjectName).ToString();
			}
		};

		bool bIsValid;
		if (Type == "ClassRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Class);
			bIsValid = LoadObject<UClass>(nullptr, *NewName) != nullptr;
		}
		else if (Type == "StructRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Struct);
			bIsValid = LoadObject<UScriptStruct>(nullptr, *NewName) != nullptr;
		}
		else if (Type == "EnumRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Enum);
			bIsValid = LoadObject<UEnum>(nullptr, *NewName) != nullptr;
		}
		else if (Type == "FunctionRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Function);
			bIsValid = FindFirstObject<UFunction>(*NewName, EFindFirstObjectOptions::EnsureIfAmbiguous) != nullptr;
		}
		else if (Type == "PackageRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Package);
			bIsValid = FindPackage(nullptr, *NewName) != nullptr;
		}
		else if (Type == "PropertyRedirects")
		{
			ApplyRedirect(ECoreRedirectFlags::Type_Property);

			FString SearchName = NewName;
			int32 DelimiterIndex = 0;
			if (SearchName.FindLastChar(TEXT('.'), DelimiterIndex))
			{
				SearchName[DelimiterIndex] = TEXT(':');
			}

			bIsValid = FindFPropertyByPath(*SearchName) != nullptr;
		}
		else
		{
			ensure(false);
			return;
		}

		if (!bIsValid ||
			OldName == NewName)
		{
			continue;
		}

		NewLines.Add(FString::Printf(TEXT("+%s=(OldName=\"%s\",NewName=\"%s\")"),
			*Type,
			*OldName,
			*NewName));
	}

	for (FString& Line : NewLines)
	{
		Line = Line.TrimStartAndEnd();
	}

	for (int32 Index = 1; Index < NewLines.Num(); Index++)
	{
		if (NewLines[Index - 1].IsEmpty() &&
			NewLines[Index].IsEmpty())
		{
			NewLines.RemoveAt(Index);
			Index--;
		}
	}

	while (
		NewLines.Num() > 0 &&
		NewLines.Last().TrimStartAndEnd().IsEmpty())
	{
		NewLines.Pop();
	}

	const FString NewFile = FString::Join(NewLines, TEXT("\n"));

	FString ExistingFile;
	FFileHelper::LoadFileToString(ExistingFile, *RedirectsPath);

	// Normalize line endings
	ExistingFile.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

	if (ExistingFile.Equals(NewFile))
	{
		return;
	}

	ensure(FFileHelper::SaveStringToFile(NewFile, *RedirectsPath));
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FVoxelUtilities::GetAppDataCache()
{
	static FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")) / "UnrealEngine" / "VoxelPlugin";
	return Path;
}

void FVoxelUtilities::CleanupFileCache(const FString& Path, const int64 MaxSize)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Path, TEXT("*"), true, false);

	int64 TotalSize = 0;
	for (const FString& File : Files)
	{
		TotalSize += IFileManager::Get().FileSize(*File);
	}

	while (
		TotalSize > MaxSize &&
		ensure(Files.Num() > 0))
	{
		FString OldestFile;
		FDateTime OldestFileTimestamp;
		for (const FString& File : Files)
		{
			const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*File);

			if (OldestFile.IsEmpty() ||
				Timestamp < OldestFileTimestamp)
			{
				OldestFile = File;
				OldestFileTimestamp = Timestamp;
			}
		}
		LOG_VOXEL(Log, "Deleting %s", *OldestFile);

		TotalSize -= IFileManager::Get().FileSize(*OldestFile);
		ensure(IFileManager::Get().Delete(*OldestFile));
		ensure(Files.Remove(OldestFile));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelStackFrames FVoxelUtilities::GetStackFramesImpl(
	const bool bEnableStats,
	const int32 NumFramesToIgnore)
{
	VOXEL_FUNCTION_COUNTER_COND(bEnableStats);

	// MAX_CALLSTACK_DEPTH is 128
	TVoxelStaticArray<void*, 128> StackFrames(NoInit);

	FPlatformStackWalk::CaptureStackBackTrace(
		ReinterpretCastPtr<uint64>(StackFrames.GetData()),
		StackFrames.Num());

	int32 Depth = 0;
	while (
		Depth < StackFrames.Num() &&
		StackFrames[Depth])
	{
		Depth++;
	}

	if (Depth < NumFramesToIgnore)
	{
		UE_DEBUG_BREAK();
		return {};
	}

	FVoxelStackFrames Result;
	Result.Reserve(Depth - NumFramesToIgnore);

	for (int32 Index = NumFramesToIgnore; Index < Depth; Index++)
	{
		Result.Add_EnsureNoGrow(StackFrames[Index]);
	}

	checkVoxelSlow(Result.Num() == Depth - NumFramesToIgnore);
	return Result;
}

TVoxelArray<FString> FVoxelUtilities::StackFramesToStringImpl(
	const bool bEnableStats,
	const FVoxelStackFrames& StackFrames,
	const bool bIncludeFilenames)
{
	VOXEL_FUNCTION_COUNTER_COND(bEnableStats);

	// Make sure we don't actually ensure, StackFramesToString is called within ensures in tests
#define ensureSafe(...) (INLINE_LAMBDA \
	{ \
		const bool bValue = (__VA_ARGS__); \
		if (VOXEL_DEBUG && !bValue) \
		{ \
			UE_DEBUG_BREAK(); \
		} \
		return bValue; \
	})

	static const int32 InitializeStackWalking = INLINE_LAMBDA
	{
		VOXEL_SCOPE_COUNTER("FPlatformStackWalk::InitStackWalking");
		FPlatformStackWalk::InitStackWalking();
		return 0;
	};

	TVoxelArray<FString> Result;
	Result.Reserve(StackFrames.Num());

	for (int32 StackIndex = 0; StackIndex < StackFrames.Num(); StackIndex++)
	{
		void* Address = StackFrames[StackIndex];
		if (!Address)
		{
			continue;
		}

		ANSICHAR HumanReadableString[8192];
		HumanReadableString[0] = '\0';

		{
			VOXEL_SCOPE_COUNTER_COND(bEnableStats, "ProgramCounterToHumanReadableString");

			if (!FPlatformStackWalk::ProgramCounterToHumanReadableString(
				StackIndex,
				uint64(Address),
				HumanReadableString,
				8192))
			{
				Result.Add(FString::Printf(TEXT("%p: [failed to resolve]"), Address));
				continue;
			}
		}

		if (FString(HumanReadableString).Contains("__scrt_common_main_seh()"))
		{
			// We don't care about anything above this
			break;
		}

		const bool bPrettyPrint = INLINE_LAMBDA
		{
			if (!PLATFORM_WINDOWS)
			{
				return false;
			}

			FString String(HumanReadableString);
			if (!ensureSafe(String.RemoveFromStart(FString::Printf(TEXT("0x%p "), Address))))
			{
				return false;
			}

			{
				int32 Index = String.Find(".dll!");
				if (Index == -1)
				{
					Index = String.Find(".exe!");
				}
				if (!ensureSafe(Index != -1))
				{
					return false;
				}

				Index += 5;

				String = String.RightChop(Index);
			}

			FString Function;
			{
				const int32 Index = String.Find(" [");
				if (!ensureSafe(Index != -1))
				{
					return false;
				}

				Function = String.Left(Index);
				String = String.RightChop(Index);
			}

			if (!ensureSafe(String.RemoveFromStart(" [")) ||
				!ensureSafe(String.RemoveFromEnd("]")))
			{
				return false;
			}

			{
				FString FinalFunction;
				FinalFunction.Reserve(Function.Len());

				int32 TemplateCount = 0;
				for (const TCHAR Char : Function)
				{
					if (Char == TEXT('<'))
					{
						TemplateCount++;
						continue;
					}
					if (Char == TEXT('>'))
					{
						TemplateCount--;
						ensureSafe(TemplateCount >= 0);
						continue;
					}

					if (TemplateCount == 0)
					{
						FinalFunction += Char;
					}
				}

				Function = MoveTemp(FinalFunction);
			}

			Function.RemoveFromStart("`");
			Function.ReplaceInline(TEXT("'::`2'::::operator()()"), TEXT("::lambda()"));

			if (Function == "UE::Core::Private::Function::TFunctionRefCaller::Call()" ||
				Function == "UE::Core::Private::Function::TFunctionRefBase::operator()()")
			{
				// Skip
				return true;
			}

			if (!bIncludeFilenames)
			{
				Result.Add(Function);
				return true;
			}

			int32 Line = 0;
			if (!String.IsEmpty())
			{
				int32 Index;
				if (!ensureSafe(String.FindLastChar(TEXT(':'), Index)))
				{
					return false;
				}

				Line = StringToInt(String.RightChop(Index + 1));
				String = String.Left(Index);
			}

			const FString FileName = FPaths::GetCleanFilename(String);

			Result.Add(FString::Printf(TEXT("%s %s:%d"), *Function, *FileName, Line));
			return true;
		};

		if (!bPrettyPrint)
		{
			Result.Add(FString::Printf(TEXT("%p: %S"), Address, HumanReadableString));
		}
	}

#undef ensureSafe

	return Result;
}

FString FVoxelUtilities::GetPrettyCallstackImpl(
	const bool bEnableStats,
	const int32 NumFramesToIgnore)
{
	VOXEL_FUNCTION_COUNTER_COND(bEnableStats);

	const FVoxelStackFrames StackFrames = GetStackFramesImpl(
		bEnableStats,
		NumFramesToIgnore);

	const TVoxelArray<FString> Lines = StackFramesToStringImpl(
		bEnableStats,
		StackFrames,
		true);

	return FString::Join(Lines, TEXT("\n"));
}