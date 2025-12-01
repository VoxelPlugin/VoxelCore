// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelShaderHook.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "HAL/PlatformFileManager.h"
#include "VoxelShaderHooksManager.h"

FVoxelShaderFileData::FVoxelShaderFileData(const FString& Path, const FString& Content)
	: Path(Path)
	, Content(Content)
{
	if (Content.Find("\r\n") == -1)
	{
		if (Content.Find("\r") == -1)
		{
			LineEndingType = ELineEnding::LF;
		}
		else
		{
			LineEndingType = ELineEnding::CR;
		}
	}
	else
	{
		LineEndingType = ELineEnding::CRLF;
	}

	SplitIntoLines();
}

bool FVoxelShaderFileData::CreateShaderFileData(
	const FString& Path,
	TSharedPtr<FVoxelShaderFileData>& OutData)
{
	if (Path.IsEmpty())
	{
		return false;
	}

	FString FileContents;
	if (!ensure(FFileHelper::LoadFileToString(FileContents, *Path)))
	{
		return false;
	}

	OutData = MakeShared<FVoxelShaderFileData>(Path, FileContents);
	return true;
}

bool FVoxelShaderFileData::FindWrappedPositions(
	const TArray<FString>& LinesBefore,
	const TArray<FString>& LinesAfter,
	int32& OutBeforeEndsAtLine,
	int32& OutAfterStartsAtLine)
{
	if (!FindPositions(LinesBefore, 0, -1, nullptr, &OutBeforeEndsAtLine))
	{
		return false;
	}

	if (!FindPositions(LinesAfter, OutBeforeEndsAtLine, -1, &OutAfterStartsAtLine))
	{
		return false;
	}

	OutBeforeEndsAtLine--;

	return true;
}

bool FVoxelShaderFileData::FindPositions(
	const TArray<FString>& Lookup,
	int32 FromLine,
	int32 ToLine,
	int32* OutLookupStartLine,
	int32* OutLookupEndLine,
	int32* OutFileLineIndex)
{
	FromLine = FMath::Max(FromLine, 0);
	if (Lookup.Num() == 0)
	{
		return false;
	}

	if (FromLine + Lookup.Num() > Lines.Num())
	{
		return false;
	}

	if (ToLine == -1)
	{
		ToLine = Lines.Num() - Lookup.Num() + 1;
	}

	for (int32 Index = FromLine; Index < ToLine; Index++)
	{
		if (Lines[Index].Content != Lookup[0])
		{
			continue;
		}

		bool bMatches = true;
		for (int32 LookupIndex = 1; LookupIndex < Lookup.Num(); LookupIndex++)
		{
			if (Lines[Index + LookupIndex].Content != Lookup[LookupIndex])
			{
				bMatches = false;
				break;
			}
		}

		if (bMatches)
		{
			if (OutLookupStartLine)
			{
				*OutLookupStartLine = Index;
			}
			if (OutLookupEndLine)
			{
				*OutLookupEndLine = Index + Lookup.Num();
			}
			if (OutFileLineIndex)
			{
				*OutFileLineIndex = Lines[Index].LineIndex;
			}
			return true;
		}
	}

	return false;
}

void FVoxelShaderFileData::UpdateContent(const FString& NewContent)
{
	Content = NewContent;
	SplitIntoLines();
}

FString FVoxelShaderFileData::GetPartOfContent(const int32 FromLine, const int32 ToLine)
{
	return Content.Mid(Lines[FromLine].LineStartsAt, Lines[ToLine].LineEndsAt - Lines[FromLine].LineStartsAt);
}

const FString& FVoxelShaderFileData::GetLineEnding() const
{
	switch (LineEndingType)
	{
	case ELineEnding::CR: return STATIC_FSTRING("\r");
	case ELineEnding::LF: return STATIC_FSTRING("\n");
	case ELineEnding::CRLF: return STATIC_FSTRING("\r\n");
	default: ensure(false); return STATIC_FSTRING("\n");
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelShaderFileData::SplitIntoLines()
{
	Lines = {};
	int32 LineIndex = 1;
	int32 LineStartedAt = 0;

	const char LineEndingChar = LineEndingType == ELineEnding::CR ? '\r' : '\n';

	FString LastLine;
	for (int32 Index = 0; Index < Content.Len(); Index++)
	{
		if (Content[Index] != LineEndingChar)
		{
			LastLine += Content[Index];
			continue;
		}

		const int32 LineEndsAt = Index + 1;

		LastLine = LastLine.TrimStartAndEnd();
		if (LastLine.Len() > 0)
		{
			Lines.Add({ LastLine.TrimStartAndEnd(), LineStartedAt, LineEndsAt, LineIndex });
		}
		else
		{
			if (Lines.Num() > 0)
			{
				Lines.Last().LineEndsAt = LineEndsAt;
			}
		}

		LineStartedAt = LineEndsAt;
		LastLine = {};

		LineIndex++;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelShaderHook::FVoxelShaderHook(
	const FVoxelGuid& Guid,
	const FString& ShaderFile,
	const FString& LinesBefore,
	const FString& LinesAfter,
	const FString& LinesExpected,
	const FString& NewLines)
	: ShaderGuid(Guid)
{
	Path = GetShaderSourceFilePath(ShaderFile);
	if (!ensure(!ShaderFile.IsEmpty()))
	{
		return;
	}

	Path = FPaths::ConvertRelativePathToFull(Path);
	if (!ensure(!Path.IsEmpty()))
	{
		return;
	}

	const auto ParseString = [](FString Content, TArray<FString>& OutOriginalContent, TArray<FString>& OutLookup)
	{
		Content.ReplaceInline(TEXT("##"), TEXT("~~~PRAGMA~~~"));

		if (Content.Contains("#"))
		{
			checkf(false, TEXT("Shader hooks need to use ## for pragmas. Regex: ([^#]|^)#([^#]) -> $1##$2"));
		}

		Content.ReplaceInline(TEXT("~~~PRAGMA~~~"), TEXT("#"));

		for (int32 Version = MIN_VOXEL_ENGINE_VERSION; Version <= MAX_VOXEL_ENGINE_VERSION; Version++)
		{
			const FString MacroText = "UE_" + FString::FromInt(Version) + "_SWITCH(";

			while (true)
			{
				const int32 MacroStartIndex = Content.Find(*MacroText);
				if (MacroStartIndex == -1)
				{
					break;
				}

				const int32 StartIndex = MacroStartIndex + MacroText.Len();
				check(Content[StartIndex - 1] == TEXT('('));

				int32 CommaIndex = -1;
				int32 Index = StartIndex;
				int32 Num = 1;

				while (Num > 0)
				{
					checkf(Content.IsValidIndex(Index), TEXT("Missing ) in UE_5XX_SWITCH"));

					if (Num == 1 &&
						Content[Index] == TEXT(','))
					{
						checkf(CommaIndex == -1, TEXT("Invalid , in UE_5XX_SWITCH"));
						CommaIndex = Index;
					}

					if (Content[Index] == TEXT('('))
					{
						Num++;
					}
					if (Content[Index] == TEXT(')'))
					{
						Num--;
					}

					Index++;
				}
				checkf(CommaIndex != -1, TEXT("Missing , in UE_5XX_SWITCH"));

				const int32 MacroEndIndex = Index;
				const int32 EndIndex = MacroEndIndex - 1;
				const int32 SecondStartIndex = CommaIndex + 1;

				FString NewText;
				if (VOXEL_ENGINE_VERSION >= Version)
				{
					NewText = FStringView(Content).SubStr(SecondStartIndex, EndIndex - SecondStartIndex);
				}
				else
				{
					NewText = FStringView(Content).SubStr(StartIndex, CommaIndex - StartIndex);
				}

				Content.RemoveAt(MacroStartIndex, MacroEndIndex - MacroStartIndex - NewText.Len());
				FStringView(NewText).CopyString(&Content[MacroStartIndex], NewText.Len(), 0);
			}
		}

		FString LineEnding;
		if (Content.Find("\r\n") == -1)
		{
			if (Content.Find("\r") == -1)
			{
				LineEnding = "\n";
			}
			else
			{
				LineEnding = "\r";
			}
		}
		else
		{
			LineEnding = "\r\n";
		}

		Content.ParseIntoArray(OutOriginalContent, *LineEnding);

		for (FString Line : OutOriginalContent)
		{
			Line.TrimStartAndEndInline();
			if (Line.Len() == 0)
			{
				continue;
			}

			OutLookup.Add(Line);
		}
	};

	ParseString(LinesBefore, OriginalLinesBefore, LookupBefore);
	ParseString(LinesAfter, OriginalLinesAfter, LookupAfter);
	ParseString(LinesExpected, OriginalLinesExpected, LookupExpected);
	ParseString(NewLines, OriginalNewLines, LookupNewLines);
}

bool FVoxelShaderHook::IsValid() const
{
	return
		Path.Len() > 0 &&
		LookupBefore.Num() > 0 &&
		LookupAfter.Num() > 0 &&
		(LookupExpected.Num() > 0 || LookupNewLines.Num() > 0);
}

void FVoxelShaderHook::Invalidate(FVoxelShaderFileData& FileData)
{
	BeforeEndsAt = 0;
	AfterStartsAt = 0;
	if (!FileData.FindWrappedPositions(
		LookupBefore,
		LookupAfter,
		BeforeEndsAt,
		AfterStartsAt))
	{
		LOG_VOXEL(Log, "Invalid shader hook: %s Reason: failed to find context", *GetGUIDString());
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	const int32 ReplacedContentStartsAt = BeforeEndsAt + 1;
	const FString HookStartMark = "// BEGIN VOXEL SHADER HOOK ";

	bHasOriginalContent = false;
	OriginalContentStartsAt = -1;
	OriginalContentEndsAt = -1;

	// If line after 'BeforeLines' does not start with '// BEGIN VOXEL', there's no hook applied
	if (!FileData[ReplacedContentStartsAt].Content.StartsWith(HookStartMark))
	{
		// Support old applied hooks
		if (FileData[ReplacedContentStartsAt].Content == "// BEGIN VOXEL SHADER")
		{
			if (ensure(FileData[AfterStartsAt - 1].Content == "// END VOXEL SHADER"))
			{
				State = EVoxelShaderHookState::Outdated;
			}
			else
			{
				LOG_VOXEL(Log, "Invalid shader hook: %s Reason: missing legacy END VOXEL SHADER", *GetGUIDString());
				State = EVoxelShaderHookState::Invalid;
			}
			return;
		}

		if (bDeprecated)
		{
			State = EVoxelShaderHookState::Deprecated;
			return;
		}

		if (LookupExpected.Num() == 0)
		{
			if (BeforeEndsAt + 1 == AfterStartsAt)
			{
				State = EVoxelShaderHookState::NotApplied;
			}
			else
			{
				State = EVoxelShaderHookState::Invalid;
			}
		}
		else
		{
			if (AfterStartsAt - ReplacedContentStartsAt != LookupExpected.Num())
			{
				State = EVoxelShaderHookState::Invalid;
			}
			else
			{
				for (int32 Index = 0; Index < LookupExpected.Num(); Index++)
				{
					if (FileData[Index + ReplacedContentStartsAt].Content != LookupExpected[Index])
					{
						State = EVoxelShaderHookState::Invalid;
						return;
					}
				}

				bHasOriginalContent = true;
				OriginalContentStartsAt = ReplacedContentStartsAt;
				OriginalContentEndsAt = AfterStartsAt - 1;
				State = EVoxelShaderHookState::NotApplied;
			}
		}
		return;
	}

	const FString GUIDString = FileData[ReplacedContentStartsAt].Content.Mid(HookStartMark.Len());

	FGuid FoundGUID;
	// GUID is invalid - manual changes done
	if (!FGuid::Parse(GUIDString, FoundGUID))
	{
		LOG_VOXEL(Log, "Invalid shader hook: %s Reason: failed to parse GUID", *GetGUIDString());
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	// GUID does not match - manual changes done
	if (FoundGUID != ShaderGuid)
	{
		LOG_VOXEL(Log, "Invalid shader hook: %s Reason: different GUID", *GetGUIDString());
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	if (FileData[AfterStartsAt - 1].Content != "// END VOXEL SHADER HOOK " + GetGUIDString())
	{
		LOG_VOXEL(Log, "Invalid shader hook: %s Reason: missing END VOXEL SHADER HOOK", *GetGUIDString());
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	// Find original content start
	const FString OldContentStartIndication = "#ifdef DISABLE_VOXEL_SHADER_HOOKS";
	for (int32 Index = ReplacedContentStartsAt + 1; Index < AfterStartsAt; Index++)
	{
		if (FileData[Index].Content == OldContentStartIndication)
		{
			OriginalContentStartsAt = Index + 1;
			break;
		}
	}

	if (OriginalContentStartsAt == -1)
	{
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	const FString OriginalContentEndingString = "#else // DISABLE_VOXEL_SHADER_HOOKS";
	for (int32 Index = OriginalContentStartsAt; Index < AfterStartsAt; Index++)
	{
		if (FileData[Index].Content == OriginalContentEndingString)
		{
			OriginalContentEndsAt = Index - 1;
			break;
		}
	}

	if (OriginalContentEndsAt == -1)
	{
		State = EVoxelShaderHookState::Invalid;
		return;
	}

	if (OriginalContentEndsAt >= OriginalContentStartsAt)
	{
		bHasOriginalContent = true;
	}

	if (LookupNewLines.Num() > 0)
	{
		if (!FileData.FindPositions(LookupNewLines, OriginalContentEndsAt + 2, AfterStartsAt))
		{
			State = EVoxelShaderHookState::Outdated;
			return;
		}
	}
	else
	{
		if (FileData[OriginalContentEndsAt + 2].Content != "#endif // DISABLE_VOXEL_SHADER_HOOKS")
		{
			State = EVoxelShaderHookState::Outdated;
			return;
		}
	}

	State = bDeprecated ? EVoxelShaderHookState::Outdated : EVoxelShaderHookState::Active;
}

bool FVoxelShaderHook::Apply(FVoxelShaderFileData& FileData)
{
	// If hook is deprecated, we revert the changes...
	if (bDeprecated)
	{
		if (Revert(FileData))
		{
			LOG_VOXEL(Display, "Reverted %s", *FileData.GetPath());
			return true;
		}
		else
		{
			LOG_VOXEL(Error, "Failed to revert %s", *FileData.GetPath());
			return false;
		}
	}

	// Make sure to have the newest state before applying changes
	Invalidate(FileData);

	switch (State)
	{
	case EVoxelShaderHookState::Active:
	{
		LOG_VOXEL(Display, "%s up-to-date", *FileData.GetPath());
		return false;
	}
	case EVoxelShaderHookState::Outdated: break;
	case EVoxelShaderHookState::NotApplied: break;
	case EVoxelShaderHookState::Invalid: return false;
	case EVoxelShaderHookState::Deprecated: ensure(false); return false;
	default: ensure(false); return false;
	}

	const FString BeforePart = FileData.GetContent().Mid(0, FileData[BeforeEndsAt].LineEndsAt);
	const FString AfterPart = FileData.GetContent().Mid(FileData[AfterStartsAt].LineStartsAt);

	const TArray<FString> NewContentLines = MakeNewContentPart(FileData, ENewContentType::File);
	const FString& LineEnding = FileData.GetLineEnding();
	FString NewContent;
	for (const FString& Line : NewContentLines)
	{
		NewContent += Line + LineEnding;
	}

	FileData.UpdateContent(BeforePart + NewContent + AfterPart);

	LOG_VOXEL(Display, "Updated %s", *FileData.GetPath());
	return true;
}

bool FVoxelShaderHook::Revert(FVoxelShaderFileData& FileData)
{
	// Make sure to have the newest state before reverting changes
	Invalidate(FileData);

	switch (State)
	{
	case EVoxelShaderHookState::Active: break;
	case EVoxelShaderHookState::Outdated: break;
	case EVoxelShaderHookState::NotApplied: return false;
	case EVoxelShaderHookState::Invalid: return false;
	case EVoxelShaderHookState::Deprecated: return false;
	default: ensure(false); return false;
	}

	const FString BeforePart = FileData.GetContent().Mid(0, FileData[BeforeEndsAt].LineEndsAt);
	const FString AfterPart = FileData.GetContent().Mid(FileData[AfterStartsAt].LineStartsAt);

	FString OriginalContent;
	if (bHasOriginalContent)
	{
		const int32 Length = FileData[OriginalContentEndsAt].LineEndsAt - FileData[OriginalContentStartsAt].LineStartsAt;
		if (Length > 0)
		{
			OriginalContent = FileData.GetContent().Mid(FileData[OriginalContentStartsAt].LineStartsAt, Length);
		}
	}

	FileData.UpdateContent(BeforePart + OriginalContent + AfterPart);
	return true;
}

bool FVoxelShaderHook::CreatePatch(FVoxelShaderFileData& FileData, TArray<FString>& OutPatch, int32& OutStartLine)
{
	// If state is deprecated, we don't show this hook in patch
	if (State == EVoxelShaderHookState::Deprecated ||
		State == EVoxelShaderHookState::Active)
	{
		return false;
	}

	const FString& LineEnding = FileData.GetLineEnding();

	int32 LookupBeforeStartsAt = 0;
	int32 LookupBeforeEndsAt = 0;
	int32 BeforeStartLine = -1;
	if (FileData.FindPositions(LookupBefore, 0, -1, &LookupBeforeStartsAt, &LookupBeforeEndsAt, &BeforeStartLine))
	{
		OutStartLine = BeforeStartLine;
		const FString PartOfContent = FileData.GetPartOfContent(LookupBeforeStartsAt, LookupBeforeEndsAt - 1);
		TArray<FString> BeforeLines;
		PartOfContent.ParseIntoArray(BeforeLines, *LineEnding, false);
		OutPatch.Append(BeforeLines);
	}
	else
	{
		OutPatch.Append(OriginalLinesBefore);
	}

	OutPatch.Append(MakeNewContentPart(FileData, bDeprecated ? ENewContentType::PatchDeprecated : ENewContentType::PatchApply));

	int32 LookupAfterStartsAt = 0;
	int32 LookupAfterEndsAt = 0;
	int32 AfterStartLine = -1;
	if (FileData.FindPositions(LookupAfter, BeforeEndsAt, -1, &LookupAfterStartsAt, &LookupAfterEndsAt, &AfterStartLine))
	{
		if (BeforeStartLine == -1)
		{
			OutStartLine = AfterStartLine - OriginalLinesExpected.Num() - OriginalLinesBefore.Num();
		}

		const FString PartOfContent = FileData.GetPartOfContent(LookupAfterStartsAt, LookupAfterEndsAt);
		TArray<FString> AfterLines;
		PartOfContent.ParseIntoArray(AfterLines, *LineEnding, false);
		OutPatch.Append(AfterLines);
	}
	else
	{
		OutPatch.Append(OriginalLinesAfter);
	}

	// Failed to find start line
	if (OutStartLine == -1)
	{
		OutStartLine = 0;
	}

	return true;
}

TArray<FString> FVoxelShaderHook::MakeNewContentPart(FVoxelShaderFileData& FileData, const ENewContentType Type)
{
	const FString Prefix = INLINE_LAMBDA
	{
		switch (Type)
		{
		case ENewContentType::File: return "";
		case ENewContentType::PatchApply: return "+";
		case ENewContentType::PatchDeprecated: return "-";
		default: ensure(false); return "";
		}
	};

	TArray<FString> NewContent;
	NewContent.Add(Prefix + "// BEGIN VOXEL SHADER HOOK " + GetGUIDString());
	NewContent.Add(Prefix + "// THIS IS AUTOGENERATED CODE, TO EDIT SEARCH C++ FOR A MATCHING GUID");
	NewContent.Add(Prefix);
	NewContent.Add(Prefix + "#ifdef DISABLE_VOXEL_SHADER_HOOKS");

	if (bHasOriginalContent)
	{
		const int32 Length = FileData[OriginalContentEndsAt].LineEndsAt - FileData[OriginalContentStartsAt].LineStartsAt;
		if (Length > 0)
		{
			const FString OriginalContent = FileData.GetContent().Mid(FileData[OriginalContentStartsAt].LineStartsAt, Length);
			TArray<FString> OriginalContentLines;
			OriginalContent.ParseIntoArray(OriginalContentLines, *FileData.GetLineEnding());
			for (const FString& Line : OriginalContentLines)
			{
				NewContent.Add(Line);
			}
		}
	}
	else if (Type != ENewContentType::File)
	{
		NewContent.Append(OriginalLinesExpected);
	}

	NewContent.Add(Prefix + "#else // DISABLE_VOXEL_SHADER_HOOKS");

	for (const FString& Line : OriginalNewLines)
	{
		NewContent.Add(Prefix + Line);
	}

	NewContent.Add(Prefix + "#endif // DISABLE_VOXEL_SHADER_HOOKS");
	NewContent.Add(Prefix);
	NewContent.Add(Prefix + "// END VOXEL SHADER HOOK " + GetGUIDString());
	NewContent.Add(Prefix);
	return NewContent;
}

FString FVoxelShaderHook::GetGUIDString() const
{
	return ShaderGuid.ToString(EGuidFormats::Digits);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelShaderHookGroup::Register()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	GVoxelShaderHooksManager->RegisterHook(*this);

	Invalidate();
}

void FVoxelShaderHookGroup::AddHook(FVoxelShaderHook&& Hook)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!Hook.IsValid())
	{
		return;
	}

	TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
	if (!FileData)
	{
		if (!ensure(FVoxelShaderFileData::CreateShaderFileData(Hook.GetPath(), FileData)))
		{
			return;
		}

		PathToFileData.Add(Hook.GetPath(), FileData);
	}

	Hooks.Add(MoveTemp(Hook));
}

bool FVoxelShaderHookGroup::IsEnabled() const
{
	switch (State.GetValue())
	{
	case EVoxelShaderHookState::NeverApply:
	{
		return false;
	}
	case EVoxelShaderHookState::Active:
	case EVoxelShaderHookState::Deprecated:
	{
		return true;
	}
	case EVoxelShaderHookState::Outdated:
	case EVoxelShaderHookState::NotApplied:
	case EVoxelShaderHookState::Invalid:
	{
		Voxel::GameTask([this]
		{
			if (const TSharedPtr<FVoxelNotification> Notification = WeakNotification.Pin())
			{
				Notification->ResetExpiration();
				return;
			}

			const FString Title = INLINE_LAMBDA
			{
				switch (State.GetValue())
				{
				default: ensure(false);
				case EVoxelShaderHookState::Outdated: return "Voxel Shader Hook is outdated";
				case EVoxelShaderHookState::NotApplied: return "Voxel Shader Hook is not applied";
				case EVoxelShaderHookState::Invalid: return "Voxel Shader Hook is invalid";
				}
			};


			FString Text = "Hook: " + DisplayName;
			Text += "\n\n";
			Text += "Reason: " + Description;

			const TSharedRef<FVoxelNotification> Notification = FVoxelNotification::Create_Failed(Title);
			Notification->SetSubText(Text);

			Notification->AddButton(
				"Settings",
				"Open settings",
				[]
				{
					ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
					const UVoxelShaderHooksSettings* Settings = GetDefault<UVoxelShaderHooksSettings>();
					SettingsModule.ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
				});

			Notification->ExpireAndFadeoutIn(10.f);

			WeakNotification = Notification;
		});

		return false;
	}
	default:
	{
		ensure(false);
		return false;
	}
	}
}

void FVoxelShaderHookGroup::Invalidate()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	InvalidateFilesCache();

	State = INLINE_LAMBDA
	{
		if (GetDefault<UVoxelShaderHooksSettings>()->DisabledHooks.Contains(StructName))
		{
			return EVoxelShaderHookState::NeverApply;
		}

		EVoxelShaderHookState NewState = EVoxelShaderHookState::None;
		for (FVoxelShaderHook& Hook : Hooks)
		{
			TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
			if (!ensure(FileData))
			{
				continue;
			}

			Hook.Invalidate(*FileData);
			NewState |= Hook.GetState();
		}

		// If it's single flag, return the single flag
		switch (NewState)
		{
		case EVoxelShaderHookState::Active: return EVoxelShaderHookState::Active;
		case EVoxelShaderHookState::Outdated: return EVoxelShaderHookState::Outdated;
		case EVoxelShaderHookState::NotApplied: return EVoxelShaderHookState::NotApplied;
		case EVoxelShaderHookState::Invalid: return EVoxelShaderHookState::Invalid;
		case EVoxelShaderHookState::Deprecated: return EVoxelShaderHookState::Deprecated;
		case EVoxelShaderHookState::None:
		case EVoxelShaderHookState::NeverApply: ensure(false); return EVoxelShaderHookState::Outdated;
		default: break;
		}

		// If at least one is invalid, whole hook is invalid
		if (EnumHasAnyFlags(NewState, EVoxelShaderHookState::Invalid))
		{
			return EVoxelShaderHookState::Invalid;
		}

		// If at least one is outdated or not applied, whole hook is outdated
		if (EnumHasAllFlags(NewState, EVoxelShaderHookState::Deprecated | EVoxelShaderHookState::NotApplied))
		{
			return EVoxelShaderHookState::NotApplied;
		}

		// If at least one is outdated or not applied, whole hook is outdated
		if (EnumHasAnyFlags(NewState, EVoxelShaderHookState::Outdated | EVoxelShaderHookState::NotApplied))
		{
			return EVoxelShaderHookState::Outdated;
		}

		return EVoxelShaderHookState::Active;
	};
}

void FVoxelShaderHookGroup::EnsureIsEnabled() const
{
	if (!GIsEditor)
	{
		return;
	}

	(void)IsEnabled();
}

bool FVoxelShaderHookGroup::Apply(bool* OutIsCancelled)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	InvalidateFilesCache();

	TSet<TSharedPtr<FVoxelShaderFileData>> UpdatedFiles;
	for (FVoxelShaderHook& Hook : Hooks)
	{
		const TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
		if (!ensure(FileData))
		{
			continue;
		}

		if (Hook.Apply(*FileData))
		{
			UpdatedFiles.Add(FileData);
		}
	}

	if (!IsRunningCommandlet())
	{
		const EAppReturnType::Type Result = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			EAppReturnType::Cancel,
			FText::FromString("The following files will be updated. Continue?\n\n" +
				FString::JoinBy(UpdatedFiles, TEXT("\n"), [](const TSharedPtr<FVoxelShaderFileData>& FileData)
				{
					return FileData->GetPath();
				})),
			INVTEXT("Update Files?"));

		if (Result != EAppReturnType::Yes)
		{
			if (OutIsCancelled)
			{
				*OutIsCancelled = Result == EAppReturnType::Cancel;
			}
			return false;
		}
	}

	bool bUpdated = false;
	for (const TSharedPtr<FVoxelShaderFileData>& FileData : UpdatedFiles)
	{
		bUpdated |= ExecuteShaderUpdate(*FileData);
	}

	// If one of files failed to update, refresh file cache and states
	Invalidate();

	return bUpdated;
}

bool FVoxelShaderHookGroup::Revert()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	InvalidateFilesCache();

	TSet<TSharedPtr<FVoxelShaderFileData>> UpdatedFiles;
	for (FVoxelShaderHook& Hook : Hooks)
	{
		TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
		if (!ensure(FileData))
		{
			continue;
		}

		if (Hook.Revert(*FileData))
		{
			UpdatedFiles.Add(FileData);
		}
	}

	if (FMessageDialog::Open(
		EAppMsgType::YesNoCancel,
		EAppReturnType::Cancel,
		FText::FromString("The following files will be updated. Continue?\n\n" +
			FString::JoinBy(UpdatedFiles, TEXT("\n"), [](const TSharedPtr<FVoxelShaderFileData>& FileData)
			{
				return FileData->GetPath();
			})),
		INVTEXT("Update Files?")) != EAppReturnType::Yes)
	{
		return false;
	}

	bool bUpdated = false;
	for (const TSharedPtr<FVoxelShaderFileData>& FileData : UpdatedFiles)
	{
		bUpdated |= ExecuteShaderUpdate(*FileData);
	}

	// If one of files failed to update, refresh file cache and states
	Invalidate();

	return bUpdated;
}

FString FVoxelShaderHookGroup::CreatePatch(const bool bAddStyling)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	Invalidate();

	struct FData
	{
		FString Patch;
		int32 StartsAt = -1;
		int32 LengthBefore = 0;
		int32 LengthAfter = 0;
	};

	const auto PrepareLine = [bAddStyling](const FString& Style, const FString& Line)
	{
		if (!bAddStyling)
		{
			return Line + "\n";
		}

		return Style + Line + "</>\n";
	};

	TMap<TSharedPtr<FVoxelShaderFileData>, TArray<FData>> FileToPatch;
	for (FVoxelShaderHook& Hook : Hooks)
	{
		TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
		if (!ensure(FileData))
		{
			continue;
		}

		TArray<FString> PathLines;
		int32 StartsAt = 0;
		if (!Hook.CreatePatch(*FileData, PathLines, StartsAt))
		{
			continue;
		}

		int32 Additions = 0;
		int32 Removals = 0;
		int32 NormalLines = 0;
		FString Patch;
		for (const FString& Line : PathLines)
		{
			if (Line.StartsWith("+"))
			{
				Additions++;
				Patch += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_AddLine\">", Line);
			}
			else if (Line.StartsWith("-"))
			{
				Removals++;
				Patch += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_RemoveLine\">", Line);
			}
			else
			{
				NormalLines++;
				Patch += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Normal\">", Line);
			}
		}

		FileToPatch.FindOrAdd(FileData).Add({ Patch, StartsAt, NormalLines + Removals, NormalLines + Additions });
	}

	FString Result;
	for (auto& It : FileToPatch)
	{
		const FString& Path = It.Key->GetPath();
		Result += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta1\">", "Index: " + Path);
		Result += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta2\">", "===================================================================");
		Result += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta1\">", "diff --git a/" + Path + " b/" + Path);
		Result += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta2\">", "--- a/" + Path);
		Result += PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta2\">", "+++ b/" + Path);

		It.Value.Sort([](const FData& A, const FData& B)
		{
			return A.StartsAt < B.StartsAt;
		});

		int32 ChangedLines = 0;
		for (const FData& Data : It.Value)
		{
			Result +=
				PrepareLine("<TextStyle StyleSet=\"VoxelStyle\" Style=\"DiffText_Meta3\">",
					"@@ "
					"-" + LexToString(Data.StartsAt) + "," + LexToString(Data.LengthBefore) + " "
					"+" + LexToString(Data.StartsAt + ChangedLines) + "," + LexToString(Data.LengthAfter));
			Result += Data.Patch;

			ChangedLines += Data.LengthAfter - Data.LengthBefore;
		}
	}

	return Result;
}

void FVoxelShaderHookGroup::InvalidateFilesCache()
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	PathToFileData = {};

	for (FVoxelShaderHook& Hook : Hooks)
	{
		TSharedPtr<FVoxelShaderFileData> FileData = PathToFileData.FindRef(Hook.GetPath());
		if (FileData)
		{
			continue;
		}

		if (!ensure(FVoxelShaderFileData::CreateShaderFileData(Hook.GetPath(), FileData)))
		{
			continue;
		}

		PathToFileData.Add(Hook.GetPath(), FileData);
	}
}

bool FVoxelShaderHookGroup::ExecuteShaderUpdate(const FVoxelShaderFileData& FileData) const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	const FString Path = FileData.GetPath();
	if (Path.IsEmpty())
	{
		return false;
	}

	const FString FileName = FPaths::GetCleanFilename(Path);

	if (IFileManager::Get().IsReadOnly(*Path))
	{
		INLINE_LAMBDA
		{
			ISourceControlProvider & Provider = ISourceControlModule::Get().GetProvider();
			if (!Provider.IsEnabled())
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly but no source control provider: manually setting it to not-readonly", Path);
				return;
			}

			const TSharedPtr<ISourceControlState> NewState = Provider.GetState(*Path, EStateCacheUsage::ForceUpdate);
			if (!NewState)
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly but failed to get source control state: manually setting it to not-readonly", Path);
				return;
			}
			if (!NewState->IsSourceControlled())
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly but file is not source controlled: manually setting it to not-readonly", Path);
				return;
			}

			if (NewState->IsCheckedOut())
			{
				return;
			}

			if (!NewState->CanCheckout())
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly but file cannot be checked out: manually setting it to not-readonly", Path);
				return;
			}

			TArray<FString> FilesToBeCheckedOut;
			FilesToBeCheckedOut.Add(Path);
			if (Provider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Succeeded)
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly but file failed to be checked out: manually setting it to not-readonly", Path);
				return;
			}

			VOXEL_MESSAGE(Info, "{0} checked out", Path);

			if (IFileManager::Get().IsReadOnly(*Path))
			{
				VOXEL_MESSAGE(Warning, "{0} is readonly after check out: manually setting it to not-readonly", Path);
				return;
			}

			ensure(!IFileManager::Get().IsReadOnly(*Path));
		};
	}

	if (IFileManager::Get().IsReadOnly(*Path))
	{
		if (!ensure(FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, false)))
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				EAppReturnType::Ok,
				FText::FromString("Failed to clear readonly flag on " + Path),
				FText::FromString(FileName + " is out of date"));

			return false;
		}
	}

	FString OriginalContent;
	FFileHelper::LoadFileToString(OriginalContent, *Path);

	if (!FFileHelper::SaveStringToFile(FileData.GetContent(), *Path, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			EAppReturnType::Ok,
			FText::FromString("Failed to write " + Path),
			FText::FromString(FileName + " is out of date"));

		return false;
	}

	VOXEL_MESSAGE(Info, "Shader updated: {0}", Path);

	FString BackupFilename = FDateTime::Now().ToString(TEXT("%Y%m%d%H%M%S")) + "_" + Path;
	BackupFilename.ReplaceInline(TEXT("/"), TEXT("_"));
	BackupFilename.ReplaceInline(TEXT(":"), TEXT("_"));

	FFileHelper::SaveStringToFile(
		OriginalContent,
		*(FPaths::EngineDir() / "Saved" / "VoxelShaderHooks" / BackupFilename),
		FFileHelper::EEncodingOptions::ForceUTF8);

	return true;
}
#endif