// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/VoxelSingleton.h"
#include "VoxelMinimal/VoxelMessageFactory.h"
#include "VoxelMinimal/VoxelCriticalSection.h"
#include "VoxelMinimal/Containers/VoxelMap.h"

class FVoxelMessageManager;

enum class EVoxelMessageSeverity
{
	Info,
	Warning,
	Error
};

class IVoxelMessageConsumer
{
public:
	virtual ~IVoxelMessageConsumer() = default;

	virtual void LogMessage(const TSharedRef<FVoxelMessage>& Message) = 0;
};

class FVoxelMessageSinkConsumer : public IVoxelMessageConsumer
{
public:
	virtual void LogMessage(const TSharedRef<FVoxelMessage>& Message) override
	{
	}
};

class VOXELCORE_API FVoxelScopedMessageConsumer
{
public:
	explicit FVoxelScopedMessageConsumer(TWeakPtr<IVoxelMessageConsumer> MessageConsumer);
	explicit FVoxelScopedMessageConsumer(TFunction<void(const TSharedRef<FVoxelMessage>&)> LogMessage);
	~FVoxelScopedMessageConsumer();

private:
	TSharedPtr<IVoxelMessageConsumer> TempConsumer;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

extern VOXELCORE_API FVoxelMessageManager* GVoxelMessageManager;

class VOXELCORE_API FVoxelMessageManager : public FVoxelSingleton
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageLogged, const TSharedRef<FVoxelMessage>&);
	FOnMessageLogged OnMessageLogged;

	using FGatherCallstack = TFunction<void(const TSharedRef<FVoxelMessage>& Message)>;
	TVoxelArray<FGatherCallstack> GatherCallstacks;

public:
	void LogMessage(const TSharedRef<FVoxelMessage>& Message);

private:
	FVoxelCriticalSection CriticalSection;

	struct FMessageTime
	{
		double Time = 0;
		uint64 FrameCounter = 0;
	};
	TVoxelMap<uint64, FMessageTime> HashToMessageTime_RequiresLock;

	void LogMessage_GameThread(const TSharedRef<FVoxelMessage>& Message) const;

public:
	static void LogMessageFormat(
		EVoxelMessageSeverity Severity,
		const TCHAR* Format);

	template<typename... ArgTypes>
	FORCENOINLINE static void LogMessageFormat(
		const EVoxelMessageSeverity Severity,
		const TCHAR* Format,
		ArgTypes&&... Args)
	{
		VOXEL_FUNCTION_COUNTER();

		if (NO_LOGGING)
		{
			return;
		}

		TVoxelFixedArray<TSharedRef<FVoxelMessageToken>, sizeof...(Args)> Tokens;
		VOXEL_FOLD_EXPRESSION(Tokens.Add(FVoxelMessageTokenFactory::CreateToken(Args)));
		GVoxelMessageManager->InternalLogMessageFormat(Severity, Format, Tokens);
	}

private:
	void InternalLogMessageFormat(
		EVoxelMessageSeverity Severity,
		const TCHAR* Format,
		TConstVoxelArrayView<TSharedRef<FVoxelMessageToken>> Tokens);
};

#define INTERNAL_CHECK_ARG(Name) static_assert(std::is_same_v<decltype(FVoxelMessageTokenFactory::CreateToken(Name	)), TSharedRef<FVoxelMessageToken>>, "Invalid arg passed to VOXEL_MESSAGE: " #Name);

#define VOXEL_MESSAGE(Severity, Message, ...) \
	INTELLISENSE_ONLY(VOXEL_FOREACH(INTERNAL_CHECK_ARG, 0, ##__VA_ARGS__)); \
	FVoxelMessageManager::LogMessageFormat(EVoxelMessageSeverity::Severity, TEXT(Message), ##__VA_ARGS__)