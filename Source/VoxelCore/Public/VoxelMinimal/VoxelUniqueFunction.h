// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelCoreMinimal.h"
#include "VoxelMinimal/Utilities/VoxelLambdaUtilities.h"

template<typename>
class TVoxelUniqueFunction;

template<typename>
constexpr bool IsVoxelUniqueFunction_V = false;

template<typename T>
constexpr bool IsVoxelUniqueFunction_V<TVoxelUniqueFunction<T>> = true;

template<typename ReturnType, typename... ArgTypes>
class TVoxelUniqueFunction<ReturnType(ArgTypes...)>
{
public:
	TVoxelUniqueFunction() = default;
	TVoxelUniqueFunction(decltype(nullptr)) {}

	template<typename FunctorType>
	requires
	(
		!IsVoxelUniqueFunction_V<std::decay_t<FunctorType>> &&
		std::is_invocable_v<FunctorType, ArgTypes...> &&
		(
			std::is_constructible_v<ReturnType, LambdaReturnType_T<FunctorType>> ||
			// For void
			std::is_same_v<ReturnType, LambdaReturnType_T<FunctorType>>
		)
	)
	FORCEINLINE TVoxelUniqueFunction(FunctorType&& Functor)
	{
		this->Bind(MoveTempIfPossible(Functor));
	}
	TVoxelUniqueFunction(const TVoxelUniqueFunction& Other) = delete;

	FORCEINLINE TVoxelUniqueFunction(TVoxelUniqueFunction&& Other)
		: Callable(Other.Callable)
		, Allocation(Other.Allocation)
	{
		Other.Callable = nullptr;
		Other.Allocation = nullptr;
	}
	FORCEINLINE ~TVoxelUniqueFunction()
	{
		if (Callable)
		{
			Unbind();
		}
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);
	}

	TVoxelUniqueFunction& operator=(const TVoxelUniqueFunction& Other) = delete;
	FORCEINLINE TVoxelUniqueFunction& operator=(TVoxelUniqueFunction&& Other)
	{
		if (Callable)
		{
			Unbind();
		}
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);

		Callable = Other.Callable;
		Allocation = Other.Allocation;

		Other.Callable = nullptr;
		Other.Allocation = nullptr;

		return *this;
	}

	FORCEINLINE ReturnType operator()(ArgTypes... Args) const
	{
		checkVoxelSlow(Callable && Allocation);
		return (*Callable)(Allocation, Args...);
	}

	FORCEINLINE operator bool() const
	{
		checkVoxelSlow((Callable != nullptr) == (Allocation != nullptr));
		return Callable != nullptr;
	}

private:
	struct FStorageBase
	{
		void (*Destroy)(void*);
		uint64 Padding;
	};
	template<typename FunctorType>
	struct TStorage : FStorageBase
	{
		FunctorType Functor;

		explicit TStorage(FunctorType&& Functor)
			: Functor(MoveTempIfPossible(Functor))
		{
		}

		static void CallDestroy(void* RawStorage)
		{
			static_cast<TStorage*>(RawStorage)->~TStorage();
			FMemory::Free(RawStorage);
		}
	};

	ReturnType(*Callable)(void*, ArgTypes&...) = nullptr;
	void* Allocation = nullptr;

	template<typename FunctorType>
	FORCEINLINE void Bind(FunctorType&& Functor)
	{
		checkVoxelSlow(!Callable);
		checkVoxelSlow(!Allocation);

		using FStorage = TStorage<FunctorType>;

		FStorage* Storage = new FStorage(MoveTempIfPossible(Functor));
		Storage->Destroy = &FStorage::CallDestroy;

		Callable = &VoxelCall<FunctorType, ReturnType, ArgTypes...>;

		checkStatic(offsetof(FStorage, Functor) == sizeof(FStorageBase));
		Allocation = reinterpret_cast<uint8*>(Storage) + sizeof(FStorageBase);
	}
	FORCEINLINE void Unbind()
	{
		checkVoxelSlow(Callable);
		checkVoxelSlow(Allocation);

		FStorageBase* Storage = reinterpret_cast<FStorageBase*>(static_cast<uint8*>(Allocation) - sizeof(FStorageBase));
		checkVoxelSlow(Storage->Destroy);
		(*Storage->Destroy)(Storage);

		Callable = nullptr;
		Allocation = nullptr;
	}
};