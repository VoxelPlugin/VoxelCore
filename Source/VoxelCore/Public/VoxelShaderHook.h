// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"

#if WITH_EDITOR
enum class EVoxelShaderHookState
{
	None = 0,
	NeverApply = 1 << 0,
	Active = 1 << 1,
	Outdated = 1 << 2,
	NotApplied = 1 << 3,
	Invalid = 1 << 4,
	Deprecated = 1 << 5,
};
ENUM_CLASS_FLAGS(EVoxelShaderHookState);

enum class EVoxelShaderHookFileState
{
	None,
	Applied,
	NotApplied,
	Invalid
};

struct VOXELCORE_API FVoxelShaderFileData
{
private:
	const FString Path;
	FString Content;

public:
	struct FLineData
	{
		FString Content;
		int32 LineStartsAt = 0;
		int32 LineEndsAt = 0;
		int32 LineIndex = 0;
	};

private:
	TArray<FLineData> Lines;

public:
	FVoxelShaderFileData(const FString& Path, const FString& Content);

	FORCEINLINE const FString& GetPath() const
	{
		return Path;
	}
	FORCEINLINE const FString& GetContent() const
	{
		return Content;
	}

	const FLineData& operator[](const int32 Index)
	{
		return Lines[Index];
	}

public:
	static bool CreateShaderFileData(
		const FString& Path,
		TSharedPtr<FVoxelShaderFileData>& OutData);

public:
	bool FindWrappedPositions(
		const TArray<FString>& LinesBefore,
		const TArray<FString>& LinesAfter,
		int32& OutBeforeEndsAtLine,
		int32& OutAfterStartsAtLine);

	bool FindPositions(
		const TArray<FString>& OriginalLookup,
		int32 FromLine,
		int32 ToLine,
		int32* OutLookupStartLine = nullptr,
		int32* OutLookupEndLine = nullptr,
		int32* OutFileLineIndex = nullptr);

	void UpdateContent(const FString& NewContent);

	FString GetPartOfContent(int32 FromLine, int32 ToLine);

	const FString& GetLineEnding() const;

private:
	enum class ELineEnding
	{
		CR,
		LF,
		CRLF
	};
	ELineEnding LineEndingType = ELineEnding::LF;
	void SplitIntoLines();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct VOXELCORE_API FVoxelShaderHook
{
public:
	const FGuid ShaderGuid;

	FVoxelShaderHook(
		const FVoxelGuid& Guid,
		const FString& ShaderFile,
		const FString& LinesBefore,
		const FString& LinesAfter,
		const FString& LinesExpected,
		const FString& NewLines);

	FVoxelShaderHook& Deprecate()
	{
		bDeprecated = true;
		return *this;
	}
	bool IsValid() const;
	FORCEINLINE const FString& GetPath() const { return Path; }
	FORCEINLINE EVoxelShaderHookState GetState() const { return State; }
	void Invalidate(FVoxelShaderFileData& FileData);
	bool Apply(FVoxelShaderFileData& FileData);
	bool Revert(FVoxelShaderFileData& FileData);
	bool CreatePatch(FVoxelShaderFileData& FileData, TArray<FString>& OutPatch, int32& OutStartLine);

private:
	enum class ENewContentType
	{
		File,
		PatchApply,
		PatchDeprecated
	};

	TArray<FString> MakeNewContentPart(FVoxelShaderFileData& FileData, ENewContentType Type);
	FString GetGUIDString() const;

private:
	FString Path;
	bool bDeprecated = false;

	// Prepared for lookup lines (trimmed and empty lines culled)
	TArray<FString> LookupBefore;
	TArray<FString> LookupAfter;
	TArray<FString> LookupExpected;
	TArray<FString> LookupNewLines;

	// Exact lines data
	TArray<FString> OriginalLinesBefore;
	TArray<FString> OriginalLinesAfter;
	TArray<FString> OriginalLinesExpected;
	TArray<FString> OriginalNewLines;

	int32 BeforeEndsAt = 0;
	int32 AfterStartsAt = 0;

	bool bHasOriginalContent = false;
	int32 OriginalContentStartsAt = -1;
	int32 OriginalContentEndsAt = -1;

	EVoxelShaderHookState State = EVoxelShaderHookState::None;
};

struct VOXELCORE_API FVoxelShaderHookGroup
{
public:
	FVoxelShaderHookGroup() = default;
	UE_NONCOPYABLE(FVoxelShaderHookGroup);

	void Register();

	void AddHook(FVoxelShaderHook&& Hook);
	bool IsEnabled() const;
	void Invalidate();
	void EnsureIsEnabled() const;
	bool Apply(bool& bOutCancel);
	bool Revert();
	FString CreatePatch(bool bAddStyling);
	FORCEINLINE EVoxelShaderHookState GetState() const { return State.GetValue(); }

private:
	void InvalidateFilesCache();
	bool ExecuteShaderUpdate(const FVoxelShaderFileData& FileData) const;

public:
	FName StructName;
	FString DisplayName;
	FString Description;
	TOptional<EVoxelShaderHookState> State;
	TArray<FVoxelShaderHook> Hooks;

private:
	TMap<FString, TSharedPtr<FVoxelShaderFileData>> PathToFileData;
	mutable TWeakPtr<SNotificationItem> WeakNotification;
};

#define DECLARE_VOXEL_SHADER_HOOK(Api, Struct) \
	struct Api Struct \
	{ \
		static FVoxelShaderHookGroup Hook; \
		\
		static void EnsureIsEnabled() \
		{ \
			Hook.EnsureIsEnabled(); \
		} \
	};

#define DEFINE_VOXEL_SHADER_HOOK(Struct, HookName, HookDescription) \
	FVoxelShaderHookGroup Struct::Hook; \
	VOXEL_RUN_ON_STARTUP(Editor, -2) \
	{ \
		FVoxelShaderHookGroup& Hook = Struct::Hook; \
		Hook.StructName = #Struct; \
		Hook.DisplayName = HookName; \
		Hook.Description = HookDescription; \
		Hook.Register(); \
	}

#define ADD_VOXEL_SHADER_HOOK(Struct, GUID, File, LinesBefore, LinesAfter, LinesExpected, NewLines) \
	VOXEL_RUN_ON_STARTUP(Editor, -1) \
	{ \
		Struct::Hook.Hooks.Add(FVoxelShaderHook(VOXEL_GUID(GUID), File, LinesBefore, LinesAfter, LinesExpected, NewLines)); \
	}

#define ADD_VOXEL_SHADER_HOOK_DEPRECATED(Struct, GUID, File, LinesBefore, LinesAfter, LinesExpected, NewLines) \
	VOXEL_RUN_ON_STARTUP(Editor, -1) \
	{ \
		Struct::Hook.Hooks.Add(FVoxelShaderHook(VOXEL_GUID(GUID), File, LinesBefore, LinesAfter, LinesExpected, NewLines).Deprecate()); \
	}

#endif