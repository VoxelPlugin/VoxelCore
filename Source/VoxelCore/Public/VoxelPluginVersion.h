// Copyright Voxel Plugin, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FVoxelPluginVersion
{
	enum class EType
	{
		Unknown,
		Release,
		Preview,
		Dev
	};
	EType Type = EType::Unknown;

	enum class EPlatform
	{
		Unknown,
		Win64,
		Mac
	};
	EPlatform Platform = EPlatform::Unknown;

    int32 Major = 0;
    int32 Minor = 0;
    int32 Hotfix = 0;
    int32 PreviewWeek = 0;
    int32 PreviewHotfix = 0;

	int32 DevCounter = 0;
	bool bNoSource = false;
	bool bDebug = false;
	int32 EngineVersion = 0;

	bool operator==(const FVoxelPluginVersion& Other) const
	{
		return
			Type == Other.Type &&
			Platform == Other.Platform &&
			Major == Other.Major &&
			Minor == Other.Minor &&
			Hotfix == Other.Hotfix &&
			PreviewWeek == Other.PreviewWeek &&
			PreviewHotfix == Other.PreviewHotfix &&
			DevCounter == Other.DevCounter &&
			bNoSource == Other.bNoSource &&
			bDebug == Other.bDebug &&
			EngineVersion == Other.EngineVersion;
	}

	bool Parse(FString Version)
	{
		if (Version.RemoveFromEnd(" (No source)"))
		{
			bNoSource = true;
		}
		if (Version.RemoveFromEnd(" (debug)"))
		{
			bDebug = true;
		}
		if (Version.RemoveFromEnd(" (No source, debug)"))
		{
			bNoSource = true;
			bDebug = true;
		}

		if (Version.RemoveFromEnd("-debug"))
		{
			bDebug = true;
		}
		if (Version.RemoveFromEnd("-nosource"))
		{
			bNoSource = true;
		}

		if (Version.RemoveFromEnd("-Win64"))
		{
			Platform = EPlatform::Win64;
		}
		if (Version.RemoveFromEnd("-Mac"))
		{
			Platform = EPlatform::Mac;
		}

		for (int32 EngineMinor = 0; EngineMinor < 99; EngineMinor++)
		{
			if (Version.RemoveFromEnd("-" + FString::FromInt(500 + EngineMinor)))
			{
				EngineVersion = 500 + EngineMinor;
				break;
			}
		}

#define CHECK(...) if (!ensure(__VA_ARGS__)) { return false; }

		if (Version.StartsWith("dev"))
		{
			Type = EType::Dev;

			FString DevCounterString = Version;
			CHECK(DevCounterString.RemoveFromStart("dev-"));

			DevCounter = FCString::Atoi(*DevCounterString);
			CHECK(FString::FromInt(DevCounter) == DevCounterString);

			return true;
		}

		TArray<FString> VersionAndPreview;
		Version.ParseIntoArray(VersionAndPreview, TEXT("p-"));
		CHECK(VersionAndPreview.Num() == 1 || VersionAndPreview.Num() == 2);

		if (VersionAndPreview.Num() == 1)
		{
			Type = EType::Release;

			TArray<FString> MinorMajorHotfix;
			VersionAndPreview[0].ParseIntoArray(MinorMajorHotfix, TEXT("."));
			CHECK(MinorMajorHotfix.Num() == 3);

			Major = FCString::Atoi(*MinorMajorHotfix[0]);
			Minor = FCString::Atoi(*MinorMajorHotfix[1]);
			Hotfix = FCString::Atoi(*MinorMajorHotfix[2]);
			CHECK(FString::FromInt(Major) == MinorMajorHotfix[0]);
			CHECK(FString::FromInt(Minor) == MinorMajorHotfix[1]);
			CHECK(FString::FromInt(Hotfix) == MinorMajorHotfix[2]);

			return true;
		}
		else
		{
			Type = EType::Preview;

			TArray<FString> MinorMajor;
			VersionAndPreview[0].ParseIntoArray(MinorMajor, TEXT("."));
			CHECK(MinorMajor.Num() == 2);

			TArray<FString> PreviewAndHotfix;
			VersionAndPreview[1].ParseIntoArray(PreviewAndHotfix, TEXT("."));
			CHECK(PreviewAndHotfix.Num() == 1 || PreviewAndHotfix.Num() == 2);

			Major = FCString::Atoi(*MinorMajor[0]);
			Minor = FCString::Atoi(*MinorMajor[1]);
			CHECK(FString::FromInt(Major) == MinorMajor[0]);
			CHECK(FString::FromInt(Minor) == MinorMajor[1]);

			PreviewWeek = FCString::Atoi(*PreviewAndHotfix[0]);
			CHECK(FString::FromInt(PreviewWeek) == PreviewAndHotfix[0]);

			if (PreviewAndHotfix.Num() == 2)
			{
				PreviewHotfix = FCString::Atoi(*PreviewAndHotfix[1]);
				CHECK(FString::FromInt(PreviewHotfix) == PreviewAndHotfix[1]);
			}

			return true;
		}
#undef CHECK
	}
	void ParseCounter(int32 Counter)
	{
		if (Counter == 0)
		{
			Type = EType::Unknown;
			return;
		}
		if (Counter < 100000)
		{
			Type = EType::Dev;
			DevCounter = Counter;
			return;
		}

		PreviewHotfix = Counter % 10;
		Counter /= 10;

		PreviewWeek = Counter % 1000;
		Counter /= 1000;

		Hotfix = Counter % 10;
		Counter /= 10;

		Minor = Counter % 10;
		Counter /= 10;

		Major = Counter % 10;
		ensure(Counter == Major);

		Type = PreviewWeek == 999 ? EType::Release : EType::Preview;
	}
	FString GetBranch() const
	{
		if (Type == EType::Unknown)
		{
			return "unknown";
		}
		if (Type == EType::Dev)
		{
			return "dev";
		}
		return FString::Printf(TEXT("%d.%d"), Major, Minor);
	}
	int32 GetCounter() const
	{
		if (Type == EType::Unknown)
		{
			return 0;
		}
		if (Type == EType::Dev)
		{
			return DevCounter;
		}

		int32 Counter = Major;

		Counter *= 10;
		Counter += Minor;

		Counter *= 10;
		Counter += Type == EType::Preview ? 0 : Hotfix;

		Counter *= 1000;
		Counter += Type == EType::Preview ? PreviewWeek : 999;

		Counter *= 10;
		Counter += Type == EType::Preview ? PreviewHotfix : 0;

		return Counter;
	}
	FString ToString(
		const bool bPrintEngineVersion,
		const bool bPrintPlatform,
		const bool bPrintNoSource,
		const bool bPrintDebug) const
	{
		FString Result;
		if (Type == EType::Unknown)
		{
			return "Unknown";
		}
		else if (Type == EType::Release)
		{
			Result = FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Hotfix);
		}
		else if (Type == EType::Preview)
		{
			ensure(Hotfix == 0);
			Result = FString::Printf(TEXT("%d.%dp-%d.%d"), Major, Minor, PreviewWeek, PreviewHotfix);
		}
		else
		{
			check(Type == EType::Dev);
			Result = FString::Printf(TEXT("dev-%d"), DevCounter);
		}

		if (bPrintEngineVersion)
		{
			Result += "-" + FString::FromInt(EngineVersion);
		}

		if (bPrintPlatform)
		{
			if (Platform == EPlatform::Unknown)
			{
				// Nothing
			}
			else if (Platform == EPlatform::Win64)
			{
				Result += "-Win64";
			}
			else
			{
				ensure(Platform == EPlatform::Mac);
				Result += "-Mac";
			}
		}

		if (bPrintNoSource)
		{
			if (bNoSource)
			{
				Result += "-nosource";
			}
		}
		if (bPrintDebug)
		{
			if (bDebug)
			{
				Result += "-debug";
			}
		}

		return Result;
	}
	FString ToString_API() const
	{
		return ToString(true, true, true, true);
	}
	FString ToString_MajorMinor() const
	{
		return ToString(false, false, false, false);
	}
	FString ToString_UserFacing() const
	{
		FString Result = ToString_MajorMinor();
		if (bNoSource || bDebug)
		{
			Result += " (";

			if (bNoSource)
			{
				Result += "No source";
				if (bDebug)
				{
					Result += ", ";
				}
			}
			if (bDebug)
			{
				Result += "debug";
			}

			Result += ")";
		}
		return Result;
	}
};