// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMessage.generated.h"

class IMessageToken;
class FTokenizedMessage;
namespace EMessageSeverity { enum Type : int32; }

USTRUCT()
struct VOXELCORE_API FVoxelMessageToken
	: public FVoxelVirtualStruct
	, public TSharedFromThis<FVoxelMessageToken>
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	virtual uint32 GetHash() const VOXEL_PURE_VIRTUAL({});
	virtual FString ToString() const VOXEL_PURE_VIRTUAL({});
	virtual TSharedRef<IMessageToken> GetMessageToken() const;
	virtual void GetObjects(TSet<const UObject*>& OutObjects) const {}
	virtual bool TryMerge(const FVoxelMessageToken& Other) { return false; }
};

USTRUCT()
struct VOXELCORE_API FVoxelMessageToken_Group : public FVoxelMessageToken
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	void AddText(const FString& Text);
	void AddToken(const TSharedRef<FVoxelMessageToken>& Token);

	const TVoxelArray<TSharedRef<FVoxelMessageToken>>& GetTokens() const
	{
		return Tokens;
	}

private:
	TVoxelArray<TSharedRef<FVoxelMessageToken>> Tokens;
};

class VOXELCORE_API FVoxelMessage : public TSharedFromThis<FVoxelMessage>
{
public:
	static TSharedRef<FVoxelMessage> Create(EVoxelMessageSeverity Severity);

public:
	void AddText(const FString& Text);
	void AddToken(const TSharedRef<FVoxelMessageToken>& Token);

public:
	EVoxelMessageSeverity GetSeverity() const
	{
		return Severity;
	}
	const TVoxelArray<TSharedRef<FVoxelMessageToken>>& GetTokens() const
	{
		return Tokens;
	}

	template<typename T>
	TSet<const T*> GetTypedOuters() const
	{
		TSet<const T*> Result;
		for (const UObject* Outer : GetObjects())
		{
			if (const T* TypedOuter = Cast<T>(Outer))
			{
				Result.Add(TypedOuter);
				continue;
			}
			if (const T* TypedOuter = Outer->GetTypedOuter<T>())
			{
				Result.Add(TypedOuter);
				continue;
			}
		}
		return Result;
	}

public:
	uint64 GetHash() const;
	FString ToString() const;
	TSet<const UObject*> GetObjects() const;
	EMessageSeverity::Type GetMessageSeverity() const;
	TSharedRef<FTokenizedMessage> CreateTokenizedMessage() const;

private:
	const EVoxelMessageSeverity Severity;
	TVoxelArray<TSharedRef<FVoxelMessageToken>> Tokens;

	explicit FVoxelMessage(const EVoxelMessageSeverity Severity)
		: Severity(Severity)
	{
	}
};