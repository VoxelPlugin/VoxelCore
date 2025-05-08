// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/WeakInterfacePtr.h"
#include "VoxelMinimal/VoxelObjectPtr.h"
#include "VoxelMinimal/Containers/VoxelArray.h"

class UEdGraph;
class UEdGraphPin;
class UEdGraphNode;
class FVoxelMessage;
struct FVoxelMessageToken;

template<typename T, typename = void>
struct TVoxelMessageTokenFactory
{
	static void CreateToken(const T& Value);
};

struct CVoxelMessageTokenFactoryHasCreateMessageToken
{
	template<typename T>
	auto Requires() -> decltype(std::declval<T>().CreateMessageToken());
};

struct VOXELCORE_API FVoxelMessageTokenFactory
{
public:
	template<typename T>
	static auto CreateToken(T&& Value)
	{
		using Type = std::decay_t<T>;

		if constexpr (HasFactory<Type>)
		{
			return ReturnToken(TVoxelMessageTokenFactory<Type>::CreateToken(Value));
		}
		else if constexpr (std::is_pointer_v<Type>)
		{
			using InnerType = std::remove_pointer_t<Type>;

			if constexpr (
				std::is_const_v<InnerType> &&
				HasFactory<std::remove_const_t<InnerType>*>)
			{
				return ReturnToken(TVoxelMessageTokenFactory<std::remove_const_t<InnerType>*>::CreateToken(Value));
			}
			else
			{
				if (!Value)
				{
					return CreateTextToken("<null>");
				}

				return ReturnToken(CreateToken(*Value));
			}
		}
		else if constexpr (
			std::is_const_v<Type> &&
			HasFactory<std::remove_const_t<Type>>)
		{
			return ReturnToken(TVoxelMessageTokenFactory<std::remove_const_t<Type>>::CreateToken(Value));
		}
		else if constexpr (TModels<CVoxelMessageTokenFactoryHasCreateMessageToken, T>::Value)
		{
			return ReturnToken(Value.CreateMessageToken());
		}
		else
		{
			static_assert(std::is_void_v<T>, "Invalid arg passed to VOXEL_MESSAGE");
		}
	}

	static TSharedRef<FVoxelMessageToken> CreateTextToken(const FString& Text);
	static TSharedRef<FVoxelMessageToken> CreatePinToken(const UEdGraphPin* Pin);
	static TSharedRef<FVoxelMessageToken> CreateObjectToken(FVoxelObjectPtr WeakObject);
	static TSharedRef<FVoxelMessageToken> CreateArrayToken(const TVoxelArray<TSharedRef<FVoxelMessageToken>>& Tokens);

private:
	template<typename T>
	static constexpr bool HasFactory = std::is_same_v<decltype(TVoxelMessageTokenFactory<T>::CreateToken(std::declval<T&>())), TSharedRef<FVoxelMessageToken>>;

	// For type checking
	FORCEINLINE static TSharedRef<FVoxelMessageToken>&& ReturnToken(TSharedRef<FVoxelMessageToken>&& Token)
	{
		return MoveTemp(Token);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DECLARE(Type) \
	template<> \
	struct VOXELCORE_API TVoxelMessageTokenFactory<Type> : FVoxelMessageTokenFactory \
	{ \
		static TSharedRef<FVoxelMessageToken> CreateToken(std::add_const_t<Type>& Value); \
	};

DECLARE(FText);
DECLARE(const char*);
DECLARE(const TCHAR*);
DECLARE(FName);
DECLARE(FString);
DECLARE(FScriptInterface);

DECLARE(int8);
DECLARE(int16);
DECLARE(int32);
DECLARE(int64);

DECLARE(uint8);
DECLARE(uint16);
DECLARE(uint32);
DECLARE(uint64);

DECLARE(float);
DECLARE(double);

DECLARE(bool);

#undef DECLARE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelMessageTokenFactory<TVoxelObjectPtr<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TVoxelObjectPtr<T> Object)
	{
		// Don't require including T
		return FVoxelMessageTokenFactory::CreateObjectToken(Object);
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TWeakInterfacePtr<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TWeakInterfacePtr<T> Interface)
	{
		return FVoxelMessageTokenFactory::CreateObjectToken(Interface.GetWeakObjectPtr());
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<T, std::enable_if_t<std::derived_from<T, UObject>>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const T* Object)
	{
		if constexpr (TModels<CVoxelMessageTokenFactoryHasCreateMessageToken, T>::Value)
		{
			if (Object)
			{
				return Object->CreateMessageToken();
			}
		}

		ensure(IsInGameThread());
		return FVoxelMessageTokenFactory::CreateObjectToken(Object);
	}
	static TSharedRef<FVoxelMessageToken> CreateToken(const T& Object)
	{
		return TVoxelMessageTokenFactory::CreateToken(&Object);
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<T, std::enable_if_t<!std::derived_from<T, UObject> && std::derived_from<T, IInterface>>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const T* Interface)
	{
		if constexpr (TModels<CVoxelMessageTokenFactoryHasCreateMessageToken, T>::Value)
		{
			if (Interface)
			{
				return Interface->CreateMessageToken();
			}
		}

		ensure(IsInGameThread());
		return FVoxelMessageTokenFactory::CreateObjectToken(Cast<UObject>(Interface));
	}
	static TSharedRef<FVoxelMessageToken> CreateToken(const T& Interface)
	{
		return TVoxelMessageTokenFactory::CreateToken(&Interface);
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TObjectPtr<T>, std::enable_if_t<std::derived_from<T, UObject>>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TObjectPtr<T>& Object)
	{
		ensure(IsInGameThread());
		return TVoxelMessageTokenFactory<T>::CreateToken(Object.Get());
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TSoftObjectPtr<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TSoftObjectPtr<const T>& Object)
	{
		if (ensure(IsInGameThread()))
		{
			return FVoxelMessageTokenFactory::CreateObjectToken(ReinterpretCastRef<FSoftObjectPtr>(Object).LoadSynchronous());
		}
		else
		{
			return FVoxelMessageTokenFactory::CreateToken("SoftObjectPtr");
		}
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TSubclassOf<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TSubclassOf<T>& Class)
	{
		return FVoxelMessageTokenFactory::CreateObjectToken(Class);
	}
};

template<>
struct TVoxelMessageTokenFactory<UEdGraphPin>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const UEdGraphPin* Pin)
	{
		return FVoxelMessageTokenFactory::CreatePinToken(Pin);
	}
	static TSharedRef<FVoxelMessageToken> CreateToken(const UEdGraphPin& Pin)
	{
		return FVoxelMessageTokenFactory::CreatePinToken(&Pin);
	}
};

template<typename T, typename Allocator>
struct TVoxelMessageTokenFactory<TArray<T, Allocator>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TArray<T, Allocator>& Array)
	{
		TVoxelArray<TSharedRef<FVoxelMessageToken>> Tokens;
		for (const T& Value : Array)
		{
			Tokens.Add(FVoxelMessageTokenFactory::CreateToken(Value));
		}
		return FVoxelMessageTokenFactory::CreateArrayToken(Tokens);
	}
};

template<typename T, typename Allocator>
struct TVoxelMessageTokenFactory<TVoxelArray<T, Allocator>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TVoxelArray<T, Allocator>& Array)
	{
		return TVoxelMessageTokenFactory<TArray<T, Allocator>>::CreateToken(Array);
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TSharedPtr<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TSharedPtr<T>& Value)
	{
		return FVoxelMessageTokenFactory::CreateToken(Value.Get());
	}
};

template<typename T>
struct TVoxelMessageTokenFactory<TSharedRef<T>>
{
	static TSharedRef<FVoxelMessageToken> CreateToken(const TSharedRef<T>& Value)
	{
		return FVoxelMessageTokenFactory::CreateToken(Value.Get());
	}
};